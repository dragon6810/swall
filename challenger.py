#!/usr/bin/env python3
import argparse, os, subprocess, sys, time, threading, queue, shutil
import traceback
from datetime import datetime

# --- Config ---
BENCH_DIR    = ".bench"                          # add to .gitignore
POSITIONS_EP = os.path.join(BENCH_DIR, "positions.epd")
PGN_DIR      = os.path.join(BENCH_DIR, "pgn")
CRASH_DIR    = os.path.join(BENCH_DIR, "crashes")
GAMES        = 100                                # total games cap
MOVETIME_MS  = 50                                 # fixed movetime per move

def log(msg): print(msg, flush=True)

def run(cmd, cwd=None, shell=True, check=True, desc=None):
    where = cwd or os.getcwd()
    log(f"$ ({where}) {cmd}" + (f"  # {desc}" if desc else ""))
    p = subprocess.run(cmd, cwd=cwd, shell=shell, text=True,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if p.stdout: log(p.stdout.rstrip())
    if check and p.returncode != 0:
        raise RuntimeError(f"Command failed (exit {p.returncode}) in {where}:\n{cmd}\n{p.stdout}")
    return p.stdout

def clone_checkout(repo_root, commit, dest):
    run(f"git clone --quiet {repo_root} {dest}", desc="clone")
    run(f"git -c advice.detachedHead=false checkout --quiet {commit}", cwd=dest, desc=f"checkout {commit}")

def try_build(repo_dir):
    tried = []
    for cmd in ("./cleanbuild.sh", "bash ./cleanbuild.sh", "bash scripts/cleanbuild.sh"):
        tried.append(cmd)
        try:
            run(cmd, cwd=repo_dir, desc="build")
            return
        except Exception as e:
            last_err = e
    raise RuntimeError("Build failed.\nTried: " + ", ".join(tried)) from last_err

def ensure_file(path, what):
    if not os.path.exists(path):
        raise FileNotFoundError(f"{what} not found: {path}")

def now_stamp():
    return datetime.now().strftime("%Y%m%d-%H%M%S")

# --- tiny thread-safe event ring for unified transcript (> stdin, < stdout) ---
class EventRing:
    def __init__(self, max_events=20000):
        self._buf = []
        self._max = max_events
        self._lock = threading.Lock()
    def add(self, line: str):
        # line already includes prefix "> " or "< " and trailing newline
        with self._lock:
            self._buf.append(line if line.endswith("\n") else line + "\n")
            if len(self._buf) > self._max:
                del self._buf[:len(self._buf) - self._max]
    def text(self) -> str:
        with self._lock:
            return "".join(self._buf)

# ---------- Load positions ----------
def load_positions(epd_path):
    if not os.path.exists(epd_path):
        return None
    fens = []
    with open(epd_path, "r", encoding="utf-8", errors="ignore") as f:
        for raw in f:
            line = raw.strip()
            if not line or line.startswith("#"): continue
            if line.lower() == "startpos":
                fens.append("startpos"); continue
            parts = line.split()
            if len(parts) >= 4:
                if len(parts) < 6:
                    parts = (parts + ["0", "1"])[:6]
                fens.append(" ".join(parts[:6]))
    return fens

# ---------- UCI wrapper ----------
class Reader(threading.Thread):
    def __init__(self, proc, log_prefix, event_sink=None):
        super().__init__(daemon=True)
        self.proc = proc
        self.q = queue.Queue()
        self.log_prefix = log_prefix
        self.event_sink = event_sink
    def run(self):
        for line in iter(self.proc.stdout.readline, b""):
            s = line.decode(errors="replace")
            if self.event_sink:
                self.event_sink.add("< " + s)  # capture stdout
            # log(f"{self.log_prefix} {s.rstrip()}")
            self.q.put(s)
        self.q.put(None)
    def readline(self, timeout=None):
        try: return self.q.get(timeout=timeout)
        except queue.Empty: return None

class EngineCrashed(RuntimeError):
    def __init__(self, name, returncode, ctx, transcript_text):
        super().__init__(f"{name} crashed ({returncode}) during {ctx}")
        self.name = name
        self.returncode = returncode
        self.ctx = ctx
        self.transcript_text = transcript_text

class UCIEngine:
    def __init__(self, argv, name, workdir, commit="<unknown>"):
        self.argv = argv
        self.name = name
        self.commit = commit
        self.workdir = workdir
        self.proc = None
        self.reader = None
        self.events = EventRing(max_events=40000)  # unified transcript buffer
    def start(self):
        log(f"[{self.name}] start: cwd={self.workdir} cmd={' '.join(self.argv)}")
        self.proc = subprocess.Popen(
            self.argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, cwd=self.workdir, bufsize=0
        )
        self.reader = Reader(self.proc, f"[{self.name}]", event_sink=self.events)
        self.reader.start()
        self.send("uci"); self._expect("uciok", 10, ctx="uci handshake")
        # Tune options here if desired
        self.send("isready"); self._expect("readyok", 100, ctx="isready")
    def stop(self):
        if self.proc and self.proc.poll() is None:
            try:
                self.send("quit"); self.proc.wait(timeout=1)
            except Exception:
                self.proc.kill()
        log(f"[{self.name}] stopped")
    def send(self, line):
        if self.proc.poll() is not None:
            raise EngineCrashed(self.name, self.proc.returncode, f"send('{line}')", self.events.text())
        # record stdin into transcript (oldest-first chronologically)
        self.events.add("> " + line + "\n")
        self.proc.stdin.write((line + "\n").encode()); self.proc.stdin.flush()
    def _expect(self, token, timeout, ctx):
        deadline = time.time() + timeout
        while time.time() < deadline:
            if self.proc.poll() is not None:
                raise EngineCrashed(self.name, self.proc.returncode, f"expect {token} ({ctx})", self.events.text())
            line = self.reader.readline(timeout=0.1)
            if line is None:
                if self.proc.poll() is not None:
                    raise EngineCrashed(self.name, self.proc.returncode, f"expect {token} ({ctx})", self.events.text())
                continue
            if token in line:
                return
        # timeout (treat as failure and capture transcript)
        raise EngineCrashed(self.name, self.proc.returncode if self.proc else None, f"timeout waiting for {token} ({ctx})", self.events.text())
    def newgame(self):
        self.send("ucinewgame"); self.send("isready"); self._expect("readyok", 100, ctx="newgame")
    def bestmove_fen(self, start_fen, moves_uci, movetime_ms):
        if start_fen == "startpos":
            if moves_uci:
                self.send(f"position startpos moves {' '.join(moves_uci)}")
            else:
                self.send("position startpos")
        else:
            if moves_uci:
                self.send(f"position fen {start_fen} moves {' '.join(moves_uci)}")
            else:
                self.send(f"position fen {start_fen}")
        self.send(f"go movetime {movetime_ms}")
        deadline = time.time() + movetime_ms/1000 + 5
        best = None
        while time.time() < deadline:
            if self.proc.poll() is not None:
                raise EngineCrashed(self.name, self.proc.returncode, "go movetime (waiting for bestmove)", self.events.text())
            line = self.reader.readline(timeout=0.2)
            if line is None:
                if self.proc.poll() is not None:
                    raise EngineCrashed(self.name, self.proc.returncode, "go movetime (EOF before bestmove)", self.events.text())
                continue
            if line.startswith("bestmove"):
                parts = line.strip().split()
                if len(parts) >= 2: best = parts[1]
                break
        if best is None:
            raise EngineCrashed(self.name, self.proc.returncode if self.proc else None, "timeout/no bestmove", self.events.text())
        return best
    def transcript(self) -> str:
        return self.events.text()

# ---------- Adjudication + PGN ----------
FILES = "abcdefgh"; RANKS = "12345678"
def is_legal_uci_move(move):
    if move is None or len(move) < 4: return False
    f1,r1,f2,r2 = move[0],move[1],move[2],move[3]
    return (f1 in FILES and f2 in FILES and r1 in RANKS and r2 in RANKS)

try:
    import chess as _pc
    import chess.pgn as _pgn
    HAVE_PYCHESS = True
except Exception:
    HAVE_PYCHESS = False

def save_pgn(game_id, code, start_fen, moves_uci, white_name, black_name, result_str, a_commit, b_commit):
    if not HAVE_PYCHESS:
        log("[warn] python-chess not available; PGN logging skipped.")
        return
    board = _pc.Board() if start_fen == "startpos" else _pc.Board(start_fen)
    for u in moves_uci:
        try:
            board.push_uci(u)
        except Exception:
            break
    game = _pgn.Game.from_board(board)
    game.headers["Event"] = "swall challenger"
    game.headers["Site"]  = "local"
    game.headers["Date"]  = time.strftime("%Y.%m.%d")
    game.headers["Round"] = str(game_id)
    game.headers["White"] = white_name
    game.headers["Black"] = black_name
    game.headers["Result"] = result_str
    game.headers["WhiteEngine"] = f"A ({a_commit})"
    game.headers["BlackEngine"] = f"B ({b_commit})"
    os.makedirs(PGN_DIR, exist_ok=True)
    fname = os.path.join(PGN_DIR, f"{game_id}-{code}.pgn")
    with open(fname, "w", encoding="utf-8") as f:
        exporter = _pgn.StringExporter(headers=True, variations=False, comments=False)
        f.write(game.accept(exporter))
    log(f"[pgn] wrote {fname}")

def play_from_fen(engW, engB, start_fen, movetime_ms, max_plies=300):
    """Return (result_str, moves_uci_list)."""
    moves_uci = []
    if HAVE_PYCHESS:
        b = _pc.Board() if start_fen == "startpos" else _pc.Board(start_fen)
        sf = "startpos" if start_fen == "startpos" else b.fen()
        engW.newgame(); engB.newgame()
        while not b.is_game_over() and len(b.move_stack) < max_plies:
            side = engW if b.turn == _pc.WHITE else engB
            mv = side.bestmove_fen(sf, [m.uci() for m in b.move_stack], movetime_ms)
            if not is_legal_uci_move(mv):   # illegal/no move => side to move loses
                return ("0-1" if b.turn == _pc.WHITE else "1-0", moves_uci)
            try:
                b.push_uci(mv)
                moves_uci.append(mv)
            except Exception:
                return ("0-1" if b.turn == _pc.WHITE else "1-0", moves_uci)
        return (b.result(claim_draw=True), moves_uci)
    else:
        engW.newgame(); engB.newgame()
        for ply in range(max_plies):
            side = engW if ply % 2 == 0 else engB
            mv = side.bestmove_fen(start_fen, moves_uci, movetime_ms)
            if not is_legal_uci_move(mv):
                return ("0-1" if ply % 2 == 0 else "1-0", moves_uci)
            moves_uci.append(mv)
        return ("1/2-1/2", moves_uci)

def result_code(result_str, a_is_white):
    if result_str == "1/2-1/2": return "d"
    if result_str == "1-0":
        return "wa" if a_is_white else "wb"
    else:
        return "wb" if a_is_white else "wa"

def write_crash_log(engine_name, commit, transcript_text, returncode=None):
    os.makedirs(CRASH_DIR, exist_ok=True)
    rc_part = f"rc{returncode}" if returncode is not None else "rcNA"
    # keep all context in filename; file content is ONLY the chronological transcript
    fname = os.path.join(CRASH_DIR, f"{now_stamp()}-{engine_name}-{commit[:8]}-{rc_part}.log")
    with open(fname, "w", encoding="utf-8") as f:
        f.write(transcript_text if transcript_text else "")
    log(f"[crash] wrote {fname}")

# ---------- Main ----------
def main():
    parser = argparse.ArgumentParser(description="Bench two commits of this repo’s UCI engine.")
    parser.add_argument("commit_a")
    parser.add_argument("commit_b")
    args = parser.parse_args()

    log("=== Challenger starting ===")
    log(f"Repo: {os.getcwd()}")
    log(f"Commits: A={args.commit_a}  B={args.commit_b}")
    log(f"Work dir: {BENCH_DIR}")

    os.makedirs(BENCH_DIR, exist_ok=True)
    a_dir = os.path.join(BENCH_DIR, "A")
    b_dir = os.path.join(BENCH_DIR, "B")
    if os.path.exists(a_dir): shutil.rmtree(a_dir)
    if os.path.exists(b_dir): shutil.rmtree(b_dir)

    try:
        log(f"\nBuilding A ({args.commit_a})...")
        clone_checkout(os.getcwd(), args.commit_a, a_dir)
        try_build(a_dir)

        log(f"\nBuilding B ({args.commit_b})...")
        clone_checkout(os.getcwd(), args.commit_b, b_dir)
        try_build(b_dir)
    except Exception as e:
        log("\n[BUILD ERROR]"); log(str(e)); log(traceback.format_exc()); sys.exit(2)

    os.makedirs(os.path.join(a_dir, "run"), exist_ok=True)
    os.makedirs(os.path.join(b_dir, "run"), exist_ok=True)
    binA = os.path.join(a_dir, "bin", "swall")
    binB = os.path.join(b_dir, "bin", "swall")
    try:
        ensure_file(binA, "Engine A binary")
        ensure_file(binB, "Engine B binary")
    except Exception as e:
        log("\n[SETUP ERROR]"); log(str(e)); sys.exit(3)

    engA = UCIEngine(["../bin/swall"], "A", workdir=os.path.join(a_dir, "run"), commit=args.commit_a)
    engB = UCIEngine(["../bin/swall"], "B", workdir=os.path.join(b_dir, "run"), commit=args.commit_b)
    try:
        engA.start(); engB.start()
    except EngineCrashed as ec:
        write_crash_log(ec.name, engA.commit if ec.name == "A" else engB.commit, ec.transcript_text, ec.returncode)
        log("\n[UCI START ERROR] engine crashed while starting"); sys.exit(4)
    except Exception as e:
        log("\n[UCI START ERROR]"); log(str(e)); log(traceback.format_exc()); sys.exit(4)

    fens = load_positions(POSITIONS_EP)
    if fens:
        log(f"\nLoaded {len(fens)} positions from {POSITIONS_EP}")
    else:
        log("\nNo positions file found; using startpos")

    # Ensure PGN dir exists and is clean
    if os.path.exists(PGN_DIR):
        for fn in os.listdir(PGN_DIR):
            try:
                os.remove(os.path.join(PGN_DIR, fn))
            except Exception:
                pass
    else:
        os.makedirs(PGN_DIR, exist_ok=True)
    os.makedirs(CRASH_DIR, exist_ok=True)

    a_w = b_w = d = 0
    played = 0
    try:
        if fens:
            log(f"Running up to {GAMES} games from positions (movetime {MOVETIME_MS} ms)...\n")
            for idx, fen in enumerate(fens, start=1):
                if played >= GAMES: break

                # Game 1: A as White
                try:
                    res, moves = play_from_fen(engA, engB, fen, MOVETIME_MS)
                except EngineCrashed as ec:
                    write_crash_log(ec.name,
                                    engA.commit if ec.name == "A" else engB.commit,
                                    ec.transcript_text, ec.returncode)
                    raise
                played += 1
                code = result_code(res, a_is_white=True)
                if res == "1-0": a_w += 1
                elif res == "0-1": b_w += 1
                else: d += 1
                save_pgn(played, code, fen, moves, f"A ({args.commit_a})", f"B ({args.commit_b})", res, args.commit_a, args.commit_b)
                log(f"[{idx}/{len(fens)}] Game {played}/{GAMES}: {res}   (A:{a_w} D:{d} B:{b_w})")

                if played >= GAMES: break

                # Game 2: B as White
                try:
                    res, moves = play_from_fen(engB, engA, fen, MOVETIME_MS)
                except EngineCrashed as ec:
                    write_crash_log(ec.name,
                                    engA.commit if ec.name == "A" else engB.commit,
                                    ec.transcript_text, ec.returncode)
                    raise
                played += 1
                code = result_code(res, a_is_white=False)
                if res == "1-0": b_w += 1
                elif res == "0-1": a_w += 1
                else: d += 1
                save_pgn(played, code, fen, moves, f"B ({args.commit_b})", f"A ({args.commit_a})", res, args.commit_a, args.commit_b)
                log(f"[{idx}/{len(fens)}] Game {played}/{GAMES}: {res}   (A:{a_w} D:{d} B:{b_w})")
        else:
            log(f"Running {GAMES} games (startpos, movetime {MOVETIME_MS} ms)...\n")
            fen = "startpos"
            for g in range(1, GAMES + 1):
                a_white = (g % 2 == 1)
                try:
                    if a_white:
                        res, moves = play_from_fen(engA, engB, fen, MOVETIME_MS)
                        code = result_code(res, a_is_white=True)
                        if res == "1-0": a_w += 1
                        elif res == "0-1": b_w += 1
                        else: d += 1
                        save_pgn(g, code, fen, moves, f"A ({args.commit_a})", f"B ({args.commit_b})", res, args.commit_a, args.commit_b)
                    else:
                        res, moves = play_from_fen(engB, engA, fen, MOVETIME_MS)
                        code = result_code(res, a_is_white=False)
                        if res == "1-0": b_w += 1
                        elif res == "0-1": a_w += 1
                        else: d += 1
                        save_pgn(g, code, fen, moves, f"B ({args.commit_b})", f"A ({args.commit_a})", res, args.commit_a, args.commit_b)
                except EngineCrashed as ec:
                    write_crash_log(ec.name,
                                    engA.commit if ec.name == "A" else engB.commit,
                                    ec.transcript_text, ec.returncode)
                    raise
                played = g
                log(f"Game {g}/{GAMES}: {res}   (A:{a_w}  D:{d}  B:{b_w})")
    except EngineCrashed as ec:
        log(f"[FATAL] {ec}")
        sys.exit(5)
    finally:
        engA.stop(); engB.stop()

    log("\n=== Results ===")
    log(f"A wins: {a_w}")
    log(f"Draws : {d}")
    log(f"B wins: {b_w}")
    log("=== Done ===")

if __name__ == "__main__":
    main()
