"""Build the three independently installable SD game packages."""
from pathlib import Path
import argparse
import json
from build_game import build

ROOT = Path(__file__).resolve().parents[1]
GAMES = [('tictactoe', 'TicTacToe', 'Tic Tac Toe'), ('tetris', 'Tetris', 'Tetris'), ('snake', 'Snake', 'Snake')]


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument('--out', type=Path, default=ROOT/'build/Games')
    args = parser.parse_args()
    reports = {}
    for name, folder, title in GAMES:
        source = ROOT/'games'/name
        reports[folder] = build(source/'game.c', args.out/folder/'game.pga', title, source/'icon.png')
    (args.out/'build-report.json').write_text(json.dumps(reports, indent=2)+'\n')


if __name__ == '__main__':
    main()
