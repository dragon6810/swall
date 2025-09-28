#!/usr/bin/env python3
import argparse, os, subprocess, sys, time, threading, queue, shutil
import chess

BENCH_DIR = ".bench"   # add this to your .gitignore
GAMES = 10
MOVETIME_MS = 200      # fixed time per move

# ------------ small helpers ------------
def run(cmd, cwd=None, shell=True, check=True):
    p = subprocess.run(cmd, cwd=cwd, shell=shell, text=True,
                       stdout=subprocess.PIPE, stderr=subprocess.STDOUT)
    if check and p.returncode != 0:
        raise RuntimeError(f"Command failed in {cwd or os.getcwd()}:\n$ {cmd}\n{p.stdout}")
    return p.stdout

def clone_checkout(repo_root, commit, dest):
    run(f"git clone --quiet {repo_root} {dest}")
    run(f"git -c advice.detachedHead=false checkout --quiet {commit}", cwd=dest)

def try_build(repo_dir):
    # Try a few common variants so it "just works"
    tried = []
    for cmd in ("./cleanbuild.sh",
                "bash ./cleanbuild.sh",
                "bash scripts/cleanbuild.sh"):
        try:
            tried.append(cmd)
            run(cmd, cwd=repo_dir)
            return
        except Exception as e:
            last_err = e
    raise RuntimeError(
        "Could not build.\n"
        f"Tried: {', '.join(tried)}\n"
        "If your build script isn’t tracked by git, commit it or change the script to call the right command."
    ) from last_err

# ------------ UCI engine wrapper ------------
class Reader(threading.Thread):
    def __init__(self, proc):
        super().__init__(daemon=True)
        self.proc = proc
        self.q = queue.Queue()
    def run(self):
        for line in iter(self.proc.stdout.readline, b""):
            self.q.put(line.decode(errors="replace"))
        self.q.put(None)
    def readline(self, timeout=None):
        try: return self.q.get(timeout=timeout)
        except queue.Empty: return None

class UCIEngine:
    def __init__(self, exe_argv, name, workdir):
        self.exe_argv = exe_argv
        self.name = name
        self.workdir = workdir
        self.proc = None
        self.reader = None
    def start(self):
        self.proc = subprocess.Popen(
            self.exe_argv, stdin=subprocess.PIPE, stdout=subprocess.PIPE,
            stderr=subprocess.STDOUT, cwd=self.workdir, bufsize=0
        )
        self.reader = Reader(self.proc); self.reader.start()
        self.send("uci"); self._expect("uciok", 10)
        self.send("isready"); self._expect("readyok", 10)
    def stop(self):
        if self.proc and self.proc.poll() is None:
            try:
                self.send("quit"); self.proc.wait(timeout=1)
            except Exception:
                self.proc.kill()
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
    def bestmove(self, board, movetime_ms):
        if board.move_stack:
            moves = " ".join(m.uci() for m in board.move_stack)
            self.send(f"position startpos moves {moves}")
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
                if len(parts) >= 2: best = parts[1]
                break
        try:
            return chess.Move.from_uci(best) if best else None
        except ValueError:
            return None

# ------------ match logic ------------
def play_one(engine_w, engine_b, movetime_ms, max_plies=300):
    board = chess.Board()
    engine_w.newgame(); engine_b.newgame()
    while not board.is_game_over() and len(board.move_stack) < max_plies:
        eng = engine_w if board.turn == chess.WHITE else engine_b
        mv = eng.bestmove(board, movetime_ms)
        if mv is None or mv not in board.legal_moves:
            return "0-1" if board.turn == chess.WHITE else "1-0"
        board.push(mv)
    return board.result(claim_draw=True)

def main():
    parser = argparse.ArgumentParser(description="Bench two commits of this repo’s UCI engine.")
    parser.add_argument("commit_a")
    parser.add_argument("commit_b")
    args = parser.parse_args()

    repo_root = os.getcwd()
    os.makedirs(BENCH_DIR, exist_ok=True)

    a_dir = os.path.join(BENCH_DIR, "A")
    b_dir = os.path.join(BENCH_DIR, "B")
    if os.path.exists(a_dir): shutil.rmtree(a_dir)
    if os.path.exists(b_dir): shutil.rmtree(b_dir)

    print(f"Building A ({args.commit_a})...")
    clone_checkout(repo_root, args.commit_a, a_dir)
    try_build(a_dir)

    print(f"Building B ({args.commit_b})...")
    clone_checkout(repo_root, args.commit_b, b_dir)
    try_build(b_dir)

    # ensure run dirs exist; many repos have it already, but be forgiving
    os.makedirs(os.path.join(a_dir, "run"), exist_ok=True)
    os.makedirs(os.path.join(b_dir, "run"), exist_ok=True)

    # verify engine binaries exist after build
    binA = os.path.join(a_dir, "bin", "swall")
