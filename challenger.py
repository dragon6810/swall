#!/usr/bin/env python3
import argparse, os, subprocess, sys, time, threading, queue, shutil
import traceback

# --- Config (hard-coded on purpose) ---
BENCH_DIR   = ".bench"   # add to .gitignore
GAMES       = 100         # number of games (alternating colors)
MOVETIME_MS = 100        # fixed movetime per move

# ---------- Utilities ----------
def log(msg): print(msg, flush=True)

def run(cmd, cwd=None, shell=True, check=True, desc=None):
    where = cwd or os.getcwd()
    if desc:
        log(f"$ ({where}) {cmd}  # {desc}")
    else:
        log(f"$ ({where}) {cmd}")
    p = subprocess.run(cmd, cwd=cwd, shell=shell, text=True,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if p.stdout:
        # indent output for readability
        log(p.stdout.rstrip())
    if check and p.returncode != 0:
        raise RuntimeError(f"Command failed (exit {p.returncode}) in {where}:\n{cmd}\n{p.stdout}")
    return p.stdout

def clone_checkout(repo_root, commit, dest):
    run(f"git clone --quiet {repo_root} {dest}", desc="clone")
    run(f"git -c advice.detachedHead=false checkout --quiet {commit}", cwd=dest, desc=f"checkout {commit}")

def try_build(repo_dir):
    tried = []
    for cmd in ("./cleanbuild.sh",
                "bash ./cleanbuild.sh",
                "bash scripts/cleanbuild.sh"):
        tried.append(cmd)
        try:
            run(cmd, cwd=repo_dir, desc="build")
            return
        except Exception as e:
            last_err = e
    raise RuntimeError(
        "Build failed.\n"
        f"Tried: {', '.join(tried)}\n"
        "Make sure your build script is tracked by git at the target commits, or edit challenger.py:try_build()."
    ) from last_err

def ensure_file(path, what):
    if not os.path.exists(path):
        raise FileNotFoundError(f"{what} not found: {path}")

# ---------- UCI wrapper ----------
class Reader(threading.Thread):
    def __init__(self, proc, log_prefix):
        super().__init__(daemon=True)
        self.proc = proc
        self.q = queue.Queue()
        self.log_prefix = log_prefix
    def run(self):
        for line in iter(self.proc.stdout.readline, b""):
            s = line.decode(errors="replace")
            # Uncomment to see engine chatter live:
            # log(f"{self.log_prefix} {s.rstrip()}")
            self.q.put(s)
        self.q.put(None)
    def readline(self, timeout=None):
        try: return self.q.get(timeout=timeout)
        except queue.Empty: return None

class UCIEngine:
    def __init__(self, argv, name, workdir):
        self.argv = argv
        self.name = name
        self.workdir = workdir
        self.proc = None
        self.reader = None
    def start(self):
        log(f"[{self.name}] start: cwd={self.workdir} cmd={' '.join(self.argv)}")
        self.proc = subprocess.Popen(
            self.argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, cwd=self.workdir, bufsize=0
        )
        self.reader = Reader(self.proc, f"[{self.name}]")
        self.reader.start()
        self.send("uci"); self._expect("uciok", 10)
        self.send("isready"); self._expect("readyok", 10)
    def stop(self):
        if self.proc and self.proc.poll() is None:
            try:
                self.send("quit")
                self.proc.wait(timeout=1)
            except Exception:
                self.proc.kill()
        log(f"[{self.name}] stopped")
    def send(self, line):
        self.proc.stdin.write((line + "\n").encode()); self.proc.stdin.flush()
    def _expect(self, token, timeout):
        deadline = time.time() + timeout
        while time.time() < deadline:
            line = self.reader.readline(timeout=0.1)
            if line and token in line: return
        raise RuntimeError(f"{self.name}: timeout waiting for {token}")
    def newgame(self):
        self.send("ucinewgame"); self.send("isready"); self._expect("readyok", 10)
    def bestmove(self, moves_san, movetime_ms):
        if moves_san:
            self.send(f"position startpos moves {' '.join(moves_san)}")
        else:
            self.send("position startpos")
        self.send(f"go movetime {movetime_ms}")
        deadline = time.time() + movetime_ms/1000 + 5
        best = None
        while time.time() < deadline:
            line = self.reader.readline(timeout=0.2)
            if line is None: break
            if line.startswith("bestmove"):
                parts = line.strip().split()
                if len(parts) >= 2:
                    best = parts[1]
                break
        return best

# ---------- Minimal chess board (no external deps) ----------
FILES = "abcdefgh"
RANKS = "12345678"

def is_legal_uci_move(board, move):
    # We don’t generate moves—engines are assumed legal. We just sanity check UCI format.
    if move is None or len(move) < 4: return False
    f1, r1, f2, r2 = move[0], move[1], move[2], move[3]
    return (f1 in FILES and f2 in FILES and r1 in RANKS and r2 in RANKS)

# For results/adjudication we’ll use python-chess if available; else fall back to move-limit draw.
try:
    import chess as _pc
    HAVE_PYCHESS = True
except Exception:
    HAVE_PYCHESS = False

def play_match(engW, engB, movetime_ms, max_plies=300):
    if HAVE_PYCHESS:
        b = _pc.Board()
        engW.newgame(); engB.newgame()
        while not b.is_game_over() and len(b.move_stack) < max_plies:
            side = engW if b.turn == _pc.WHITE else engB
            best = side.bestmove([m.uci() for m in b.move_stack], movetime_ms)
            if not is_legal_uci_move(b, best):
                # illegal/no move => side to move loses
                return "0-1" if b.turn == _pc.WHITE else "1-0"
            try:
                b.push_uci(best)
            except Exception:
                return "0-1" if b.turn == _pc.WHITE else "1-0"
        return b.result(claim_draw=True)
    else:
        # Fallback: no legality or end detection; just play plies and call draw.
        engW.newgame(); engB.newgame()
        san = []
        for ply in range(max_plies):
            side = engW if ply % 2 == 0 else engB
            best = side.bestmove(san, movetime_ms)
            if not is_legal_uci_move(None, best):
                return "0-1" if ply % 2 == 0 else "1-0"
            san.append(best)
        return "1/2-1/2"

# ---------- Main ----------
def main():
    parser = argparse.ArgumentParser(description="Bench two commits of this repo’s UCI engine.")
    parser.add_argument("commit_a")
    parser.add_argument("commit_b")
    args = parser.parse_args()

    # Explicit flush banner
    log("=== Challenger starting ===")
    log(f"Repo: {os.getcwd()}")
    log(f"Commits: A={args.commit_a}  B={args.commit_b}")
    log(f"Work dir: {BENCH_DIR}")

    # Prepare bench dirs
    os.makedirs(BENCH_DIR, exist_ok=True)
    a_dir = os.path.join(BENCH_DIR, "A")
    b_dir = os.path.join(BENCH_DIR, "B")
    if os.path.exists(a_dir): shutil.rmtree(a_dir)
    if os.path.exists(b_dir): shutil.rmtree(b_dir)

    # Clone + build
    try:
        log(f"\nBuilding A ({args.commit_a})...")
        clone_checkout(os.getcwd(), args.commit_a, a_dir)
        try_build(a_dir)

        log(f"\nBuilding B ({args.commit_b})...")
        clone_checkout(os.getcwd(), args.commit_b, b_dir)
        try_build(b_dir)
    except Exception as e:
        log("\n[BUILD ERROR]")
        log(str(e))
        log(traceback.format_exc())
        sys.exit(2)

    # Ensure run/ and binary paths
    os.makedirs(os.path.join(a_dir, "run"), exist_ok=True)
    os.makedirs(os.path.join(b_dir, "run"), exist_ok=True)
    binA = os.path.join(a_dir, "bin", "swall")
    binB = os.path.join(b_dir, "bin", "swall")
    try:
        ensure_file(binA, "Engine A binary")
        ensure_file(binB, "Engine B binary")
    except Exception as e:
        log("\n[SETUP ERROR]")
        log(str(e))
        sys.exit(3)

    # Start engines
    engA = UCIEngine(["../bin/swall"], "A", workdir=os.path.join(a_dir, "run"))
    engB = UCIEngine(["../bin/swall"], "B", workdir=os.path.join(b_dir, "run"))
    try:
        engA.start(); engB.start()
    except Exception as e:
        log("\n[UCI START ERROR]")
        log(str(e))
        log(traceback.format_exc())
        sys.exit(4)

    # Play games
    a_w = b_w = d = 0
    try:
        log(f"\nRunning {GAMES} games (movetime {MOVETIME_MS} ms)...\n")
        for g in range(1, GAMES + 1):
            if g % 2 == 1:
                res = play_match(engA, engB, MOVETIME_MS)
                if res == "1-0": a_w += 1
                elif res == "0-1": b_w += 1
                else: d += 1
            else:
                res = play_match(engB, engA, MOVETIME_MS)
                if res == "1-0": b_w += 1
                elif res == "0-1": a_w += 1
                else: d += 1
            log(f"Game {g}/{GAMES}: {res}   (A:{a_w}  D:{d}  B:{b_w})")
    finally:
        engA.stop(); engB.stop()

    # Summary
    log("\n=== Results ===")
    log(f"A wins: {a_w}")
    log(f"Draws : {d}")
    log(f"B wins: {b_w}")
    log("=== Done ===")

if __name__ == "__main__":
    main()
