import argparse
import json
import os
import pathlib
import sys

from . import gmail


def print_json(payload):
    raw = json.dumps(payload, ensure_ascii=False)
    if os.environ.get("ASTREA_EMAIL_DEBUG"):
        print(f"[astrea-email] payload bytes={len(raw.encode('utf-8'))}", file=sys.stderr)
    print(raw)


def normalize_argv(argv):
    args = list(sys.argv[1:] if argv is None else argv)
    if args and args[0] == "gmail":
        args = args[1:]
    return args


def build_parser():
    parser = argparse.ArgumentParser(description="Astrea Email backend CLI")
    sub = parser.add_subparsers(dest="command", required=True)

    sub.add_parser("status")
    sub.add_parser("auth")

    list_parser = sub.add_parser("list")
    list_parser.add_argument("--folder", default="Inbox")
    list_parser.add_argument("--filter", default="all")
    list_parser.add_argument("--query", default="")
    list_parser.add_argument("--limit", default=30, type=int)
    list_parser.add_argument("--page-token", default="")
    list_parser.add_argument("--refresh", action="store_true")
    list_parser.add_argument("--no-cache", action="store_true")
    list_parser.add_argument("--cache-only", action="store_true")

    send_parser = sub.add_parser("send")
    send_parser.add_argument("--to", required=True)
    send_parser.add_argument("--subject", required=True)
    send_parser.add_argument("--body", default="")

    modify_parser = sub.add_parser("modify")
    modify_parser.add_argument("--id", required=True)
    modify_parser.add_argument("--action", required=True)

    get_parser = sub.add_parser("get")
    get_parser.add_argument("--id", required=True)
    get_parser.add_argument("--images", action="store_true")
    get_parser.add_argument("--original", action="store_true")

    preview_parser = sub.add_parser("preview")
    preview_parser.add_argument("--id", required=True)
    preview_parser.add_argument("--images", action="store_true")

    links_parser = sub.add_parser("links")
    links_parser.add_argument("--id", required=True)

    settings_parser = sub.add_parser("settings")
    settings_parser.add_argument("--set", action="append", default=[])

    notify_parser = sub.add_parser("notify")
    notify_parser.add_argument("--limit", default=20, type=int)
    notify_parser.add_argument("--state-path", type=pathlib.Path)
    notify_parser.add_argument("--no-desktop", action="store_true")
    notify_parser.add_argument("--no-clipboard", action="store_true")
    notify_parser.add_argument("--no-island", action="store_true")

    view_parser = sub.add_parser("view")
    view_parser.add_argument("--id", required=True)
    view_parser.add_argument("--no-images", action="store_true")
    view_parser.add_argument("--x", type=int)
    view_parser.add_argument("--y", type=int)
    view_parser.add_argument("--width", type=int)
    view_parser.add_argument("--height", type=int)

    return parser


def parse_setting_updates(raw_updates):
    updates = {}
    for raw in raw_updates or []:
        if "=" not in raw:
            raise gmail.GmailBridgeError(f"Invalid setting update: {raw}")
        key, value = raw.split("=", 1)
        updates[key] = gmail.parse_bool(value)
    return updates


def main(argv=None):
    args = build_parser().parse_args(normalize_argv(argv))

    try:
        if args.command == "status":
            payload = gmail.status_payload()
        elif args.command == "auth":
            payload = gmail.auth_payload()
        elif args.command == "list":
            payload = gmail.list_messages(
                args.folder,
                args.filter,
                args.query,
                args.limit,
                args.page_token,
                force_refresh=args.refresh,
                use_cache=not args.no_cache,
                cache_only=args.cache_only,
            )
        elif args.command == "send":
            payload = gmail.send_message(args.to, args.subject, args.body)
        elif args.command == "modify":
            payload = gmail.modify_message(args.id, args.action)
        elif args.command == "get":
            payload = gmail.get_message(args.id, load_remote_images=args.images, force_html=args.original)
        elif args.command == "preview":
            payload = gmail.preview_message(args.id, load_remote_images=args.images)
        elif args.command == "links":
            payload = gmail.message_links(args.id)
        elif args.command == "settings":
            payload = gmail.email_settings_payload(parse_setting_updates(args.set) if args.set else None)
        elif args.command == "notify":
            payload = gmail.poll_new_mail(
                limit=args.limit,
                state_path=args.state_path,
                desktop_notify=not args.no_desktop,
                copy_codes=not args.no_clipboard,
                island_notify=not args.no_island,
            )
        elif args.command == "view":
            payload = gmail.open_message_viewer(
                args.id,
                load_remote_images=not args.no_images,
                geometry={
                    "x": args.x,
                    "y": args.y,
                    "width": args.width,
                    "height": args.height,
                },
            )
        else:
            raise gmail.GmailBridgeError(f"Unsupported command: {args.command}")

        print_json(payload)
        return 0
    except gmail.GmailBridgeError as exc:
        print_json({"ok": False, "provider": "gmail", "messages": [], "message": str(exc)})
        return 1


if __name__ == "__main__":
    raise SystemExit(main())
