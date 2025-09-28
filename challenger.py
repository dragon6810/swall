#!/usr/bin/env python3
import argparse
import os
import sys
import tempfile
import subprocess
import time
import threading
import queue
import chess

# --- Nonblocking reader for engine stdout ---
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
        try:
            return self.q.get(timeout=timeout)
        except queue.Empty:
            return None

# --- UCI engine wrapper ---
class UCIEngine:
    def __init__(self, exe_path, name):
        self.exe_path = exe_path
        self.name = name
        self.proc = None
        self.reader = None

    def start(self):
        self.proc = subprocess.Popen(
            self.exe_path,
            stdin=subprocess.PIPE, stdout=subprocess.PIPE, stderr=subprocess.STDOUT,
            cwd="run", bufsize=0
        )
        self.reader = Reader(self.proc)
        self.reader.start()
        self.send("uci")
        self._expect("uciok")

        self.send("isready")
        self._expect("readyok")

    def stop(self):
        if self.proc and self.proc.poll() is None:
            try:
                self.send("quit")
                self.proc.wait(timeout=1)
            except Exception:
                self.proc.kill()

    def send(self, line):
        self.proc.stdin.write((line + "\n").encode())
        self.proc.stdin.flush()

    def _expect(self, token, timeout=10.0):
        deadline = time.time() + timeout
        while time.time() < deadline:
            line = self.reader.readline(timeout=0.1)
            if line and token in line:
                return
        raise RuntimeError(f"{self.name}: timeout waiting for {token}")

    def bestmove(self, board, movetime_ms=200):
        if board.move_stack:
            moves = " ".join(m.uci() for m in board.move_stack)
            self.send(f"position startpos moves {moves}")
        else:
            self.send("position startpos")
        self.send(f"go movetime {movetime_ms}")

        while True:
            line = self.reader.readline(timeout=2.0)
            if not line:
                break
            if line.startswith("bestmove"):
                parts = line.strip().split()
                if len(parts) >= 2:
                    try:
                        return chess.Move.from_uci(parts[1])
                    except ValueError:
                        return None
        return None

    def newgame(self):
        self.send("ucinewgame")
        self.send("isready")
        self._expect("readyok")

# --- Helpers ---
def run(cmd, cwd=None):
    subprocess.run(cmd, cwd=cwd, shell=True, check=True)

def clone_checkout(commit, dest):
    run(f"git clone . {dest}")
    run(f"git checkout {commit}", cwd=dest)
    run("./cleanbuild.sh", cwd=dest)

def play_game(engineA, engineB, movetime=200, max_plies=200):
    board = chess.Board()
    engineA.newgame()
    engineB.newgame()
    while not board.is_game_over() and len(board.move_stack) < max_plies:
        engine = engineA if board.turn == chess.WHITE else engineB
        move = engine.bestmove(board, movetime)
        if move is None or move not in board.legal_moves:
            return "0-1" if board.turn == chess.WHITE else "1-0"
        board.push(move)
    return board.result(claim_draw=True)

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("commit_a")
    ap.add_argument("commit_b")
    args = ap.parse_args()

    with tempfile.TemporaryDirectory() as tmp:
        dirA = os.path.join(tmp, "A")
        dirB = os.path.join(tmp, "B")
        print(f"Building A ({args.commit_a})...")
        clone_checkout(args.commit_a, dirA)
        print(f"Building B ({args.commit_b})...")
        clone_checkout(args.commit_b, dirB)

        exeA = ["../bin/swall"]
        exeB = ["../bin/swall"]

        engA = UCIEngine(exeA, "A")
        engB = UCIEngine(exeB, "B")
        engA.start()
        engB.start()

        try:
            a_wins = b_wins = draws = 0
            games = 10
            for g in range(games):
                # alternate colors
                if g % 2 == 0:
                    res = play_game(engA, engB)
                    if res == "1-0": a_wins += 1
                    elif res == "0-1": b_wins += 1
                    else: draws += 1
                else:
                    res = play_game(engB, engA)
                    if res == "1-0": b_wins += 1
                    elif res == "0-1": a_wins += 1
                    else: draws += 1
                print(f"Game {g+1}: {res}")
        finally:
            engA.stop()
            engB.stop()

        print("\n=== Results ===")
        print(f"A wins: {a_wins}, Draws: {draws}, B wins: {b_wins}")

if __name__ == "__main__":
    main()
