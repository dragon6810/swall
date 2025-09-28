#!/usr/bin/env bash

wget https://database.lichess.org/standard/lichess_db_standard_rated_2013-05.pgn.zst
zstd --decompress lichess_db_standard_rated_2013-05.pgn.zst
rm lichess_db_standard_rated_2013-05.pgn.zst
python posgen.py --pgn lichess_db_standard_rated_2013-05.pgn --engine stockfish --out .bench/positions.epd --max 400 --cp 20 --depth 16