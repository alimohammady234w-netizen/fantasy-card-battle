"""Run python main.py; --self-test verifies source or packaged runtime in isolation."""
import argparse
import json
import os
import sys


def positive_frames(value):
    try:
        frames = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError('Frame count must be an integer.') from error
    if frames < 1:
        raise argparse.ArgumentTypeError('Frame count must be at least 1.')
    return frames


def create_parser():
    parser = argparse.ArgumentParser(description='Arcana: an offline card battle game')
    parser.add_argument('--smoke', type=positive_frames, metavar='FRAMES', help='Exit after N frames (testing)')
    parser.add_argument('--headless', action='store_true', help='Use SDL dummy drivers; requires --smoke or --self-test')
    parser.add_argument('--screenshot', help='Write final rendered frame to a PNG')
    parser.add_argument('--self-test', action='store_true', help='Run isolated deployment checks, then exit')
    parser.add_argument('--report', help='Write --self-test results to this JSON file')
    return parser


def main(argv=None):
    parser = create_parser()
    args = parser.parse_args(argv)
    if args.report and not args.self_test:
        parser.error('--report requires --self-test')
    if args.self_test and (args.smoke or args.screenshot):
        parser.error('--self-test cannot be combined with --smoke or --screenshot')
    if args.headless and not (args.smoke or args.self_test):
        parser.error('--headless requires --smoke or --self-test so it can exit automatically')
    if args.headless or args.self_test:
        os.environ['SDL_VIDEODRIVER'] = 'dummy'
        os.environ['SDL_AUDIODRIVER'] = 'dummy'
    os.environ.setdefault('PYGAME_HIDE_SUPPORT_PROMPT', '1')
    if args.self_test:
        from src.systems.self_test import run_self_test, write_report
        report = run_self_test()
        if args.report:
            try:
                write_report(args.report, report)
            except OSError as error:
                if sys.stderr is not None:
                    print(f'Cannot write self-test report: {error}', file=sys.stderr)
                return 2
        if sys.stdout is not None:
            print(json.dumps(report, indent=2))
        return 0 if report['status'] == 'passed' else 1
    from src.systems.resources import setup_logging
    setup_logging()
    try:
        from src.game import Game
    except ModuleNotFoundError as error:
        if error.name == 'pygame':
            raise SystemExit('Pygame is not installed. Run: python -m pip install -r requirements.txt') from error
        raise
    Game().run(args.smoke, args.screenshot)
    return 0


if __name__ == '__main__':
    raise SystemExit(main())
