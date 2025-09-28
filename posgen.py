#!/usr/bin/env python3
import argparse, os, sys, time
import chess, chess.pgn, chess.engine

def is_balanced(board: chess.Board) -> bool:
    if board.is_check():
        return False
    pieces = sum(len(board.pieces(pt, True)) + len(board.pieces(pt, False))
                 for pt in [chess.QUEEN, chess.ROOK, chess.BISHOP, chess.KNIGHT, chess.PAWN])
    if pieces < 8:
        return False
    if not board.pieces(chess.PAWN, True) or not board.pieces(chess.PAWN, False):
        return False
    return True

def canonical_fen(board: chess.Board) -> str:
    parts = board.fen().split()
    if len(parts) < 6:
        parts = (parts + ["-","-","0","1"])[:6]
    parts[4], parts[5] = "0", "1"
    return " ".join(parts[:6])

def extract_cp(info_score, board: chess.Board):
    s = info_score
    # Try to normalize across python-chess versions
    try:
        s = s.pov(board.turn)  # may or may not exist
    except Exception:
        pass
    try:
        s = s.white()          # PovScore -> Score (older versions)
    except Exception:
        pass
    try:
        if s.is_mate():
            return None
    except Exception:
        pass
    cp = None
    try:
        cp = s.score(mate_score=None)
    except Exception:
        cp = getattr(s, "cp", None)
    if cp is None:
        return None
    return int(cp)

def eprint(msg, end="\n"):
    print(msg, end=end, file=sys.stderr, flush=True)

def sample_equal_positions(pgn_path, engine_path, out_path,
                           max_n=500, cp_window=20, min_ply=12, max_ply=70,
                           depth=16, mirror=False, progress_every=200):
    start_time = time.time()
    eng = chess.engine.SimpleEngine.popen_uci(engine_path)

    out, seen = [], set()
    games_read = 0
    evals = 0
    last_update = 0.0

    def status():
        elapsed = max(1e-6, time.time() - start_time)
        eps = evals / elapsed
        eprint(f"\r[posgen] games:{games_read}  evals:{evals}  kept:{len(out)}/{max_n}  {eps:.1f} evals/s", end="")

    with open(pgn_path, "r", encoding="utf-8", errors="ignore") as f:
        while len(out) < max_n:
            game = chess.pgn.read_game(f)
            if game is None:
                break
            games_read += 1
            board = game.board()
            for ply, move in enumerate(game.mainline_moves(), start=1):
                board.push(move)
                if ply < min_ply or ply > max_ply:
                    continue
                if not is_balanced(board):
                    continue

                info = eng.analyse(board, chess.engine.Limit(depth=depth))
                score_obj = info.get("score")
                if score_obj is None:
                    continue

                evals += 1
                if progress_every and (evals % progress_every == 0 or time.time() - last_update > 1.0):
                    status(); last_update = time.time()

                cp = extract_cp(score_obj, board)
                if cp is None:
                    continue

                if -cp_window <= cp <= cp_window:
                    fen = canonical_fen(board)
                    if fen not in seen:
                        seen.add(fen)
                        out.append(fen)
                        # print a short accept line so you know it's moving
                        eprint(f"\n[accept] {len(out)}/{max_n}  ply:{ply}  cp:{cp:+d}")
                        if mirror:
                            mb = board.mirror()
                            mfen = canonical_fen(mb)
                            if mfen not in seen and len(out) < max_n:
                                seen.add(mfen)
                                out.append(mfen)
                                eprint(f"[accept*mirror] {len(out)}/{max_n}")
                if len(out) >= max_n:
                    break

    eng.quit()
    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "w") as g:
        for fen in out:
            g.write(fen + "\n")

    # final status line + summary
    status(); eprint("")
    elapsed = max(1e-6, time.time() - start_time)
    eprint(f"[done] wrote {len(out)} positions to {out_path}  | games:{games_read} evals:{evals} in {elapsed:.1f}s ({evals/elapsed:.1f} evals/s)")

def main():
    ap = argparse.ArgumentParser(description="Extract near-equal FENs from a PGN using a UCI engine.")
    ap.add_argument("--pgn", required=True)
    ap.add_argument("--engine", required=True, help="Path or name of a UCI engine (e.g., stockfish)")
    ap.add_argument("--out", default=".bench/positions.epd")
    ap.add_argument("--max", type=int, default=500)
    ap.add_argument("--cp", type=int, default=20, help="centipawn window (±)")
    ap.add_argument("--min-ply", type=int, default=12)
    ap.add_argument("--max-ply", type=int, default=70)
    ap.add_argument("--depth", type=int, default=16)
    ap.add_argument("--mirror", action="store_true", help="also include color-mirrored positions")
    ap.add_argument("--progress-every", type=int, default=200, help="update status every N evals")
    args = ap.parse_args()

    sample_equal_positions(
        args.pgn, args.engine, args.out,
        max_n=args.max, cp_window=args.cp,
        min_ply=args.min_ply, max_ply=args.max_ply,
        depth=args.depth, mirror=args.mirror,
        progress_every=args.progress_every
    )

if __name__ == "__main__":
    main()
