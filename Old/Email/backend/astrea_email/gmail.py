import base64
import ipaddress
import hashlib
import html
import json
import os
import pathlib
import re
import secrets
import shutil
import signal
import socket
import socketserver
import subprocess
import sys
import time
import urllib.error
import urllib.parse
import urllib.request
import webbrowser
from email.message import EmailMessage
from email.utils import parseaddr, parsedate_to_datetime
from html.parser import HTMLParser
from http.server import BaseHTTPRequestHandler


SCOPES = [
    "https://www.googleapis.com/auth/gmail.modify",
    "https://www.googleapis.com/auth/gmail.send",
]

AUTH_URL = "https://accounts.google.com/o/oauth2/v2/auth"
TOKEN_URL = "https://oauth2.googleapis.com/token"
GMAIL_API = "https://gmail.googleapis.com/gmail/v1/users/me"
GMAIL_PAGE_LIMIT = 100
GMAIL_TOTAL_LIMIT = 500
INLINE_IMAGE_LIMIT_BYTES = 8 * 1024 * 1024
REMOTE_IMAGE_LIMIT = 8
REMOTE_IMAGE_MAX_BYTES = 2 * 1024 * 1024
REMOTE_IMAGE_TOTAL_BYTES = 10 * 1024 * 1024
REMOTE_IMAGE_TIMEOUT = 1.2
REMOTE_IMAGE_WORKERS = 4
HTML_IMAGE_MAX_WIDTH = 640
HTML_RENDER_CHAR_LIMIT = 70_000
HTML_RENDER_TABLE_LIMIT = 48
HTML_COMPLEX_TABLE_LIMIT = 12
HTML_COMPLEX_LAYOUT_CHAR_LIMIT = 24_000
HTML_READER_BODY_MIN_CHARS = 300
HTML_READER_BODY_LIMIT = 24_000
LINK_EXTRACT_LIMIT = 40
LINK_LABEL_LIMIT = 96
URL_TOKEN_RE = re.compile(r"<?https?://[^>\s]+>?", re.IGNORECASE)
INVISIBLE_TEXT_RE = re.compile(r"[\u034f\u061c\u180e\u200b-\u200f\u202a-\u202e\u2060-\u206f\ufeff\u00ad]")
HTML_CHARREF_RE = re.compile(r"&#(x[0-9a-fA-F]+|\d+);?")
AUTH_CODE_CONTEXT_RE = re.compile(
    r"\b(?:c[oó]digo|codigo|code|verification|verify|verifica[cç][aã]o|2fa|otp|login|entrar|sign[ -]?in)\b",
    re.IGNORECASE,
)
AUTH_STRONG_CONTEXT_RE = re.compile(
    r"\b(?:verification|verify|verifica[cç][aã]o|2fa|otp|login|entrar|sign[ -]?in)\b",
    re.IGNORECASE,
)
AUTH_CODE_CANDIDATE_RE = re.compile(r"(?<![A-Za-z0-9])([A-Za-z0-9]{4,8})(?![A-Za-z0-9])")
GMAIL_METADATA_HEADERS = ["From", "Subject", "Date"]
REMOTE_IMAGE_EXTENSIONS = {
    "image/avif": ".avif",
    "image/gif": ".gif",
    "image/jpeg": ".jpg",
    "image/jpg": ".jpg",
    "image/png": ".png",
    "image/webp": ".webp",
}

CONFIG_HOME = pathlib.Path(os.environ.get("XDG_CONFIG_HOME", "~/.config")).expanduser()
STATE_HOME = pathlib.Path(os.environ.get("XDG_STATE_HOME", "~/.local/state")).expanduser()
DEFAULT_CLIENT_SECRET = CONFIG_HOME / "AstreaOS" / "email" / "gmail_client_secret.json"
TOKEN_PATH = STATE_HOME / "Astrea" / "email" / "gmail_token.json"
CACHE_DIR = STATE_HOME / "Astrea" / "email" / "cache"
VIEWER_DIR = STATE_HOME / "Astrea" / "email" / "viewer"
EMAIL_SETTINGS_PATH = STATE_HOME / "Astrea" / "email" / "settings.json"
NOTIFICATION_STATE_PATH = STATE_HOME / "Astrea" / "email" / "notifications.json"
ISLAND_EMAIL_EVENT_PATH = STATE_HOME / "Astrea" / "island" / "email_event.json"
ASTREA_NOTIFY_CLI = pathlib.Path.home() / ".local/share/Astrea/bin/astrea-notify"
APP_ROOT = pathlib.Path(__file__).resolve().parents[2]
WEB_VIEWER_QML = APP_ROOT / "tools" / "WebMailViewer.qml"
WEB_SNAPSHOT_QML = APP_ROOT / "tools" / "WebMailSnapshot.qml"
WEB_ELECTRON_SNAPSHOT_JS = APP_ROOT / "tools" / "web_mail_snapshot_electron.js"
WEB_VIEWER_PID = VIEWER_DIR / "web-preview.pid"
WEB_PREVIEW_WIDTH = 820
WEB_PREVIEW_MAX_HEIGHT = 16000
WEB_PREVIEW_TIMEOUT = 8.0
WEB_PREVIEW_RENDER_VERSION = 3
WEB_PREVIEW_LINKS_MARKER = "__ASTREA_WEB_PREVIEW_LINKS__"
CACHE_VERSION = 5
NOTIFICATION_SEEN_LIMIT = 500
NOTIFICATION_EVENT_TTL = 8.0
SUMMARY_INDEX_FILE = "summaries.json"
DETAIL_CACHE_DIR = "details"
SUMMARY_MESSAGE_FIELDS = (
    "messageId",
    "threadId",
    "historyId",
    "folder",
    "fromName",
    "fromAddress",
    "subject",
    "preview",
    "timestamp",
    "tag",
    "starred",
    "isRead",
    "importance",
    "hasAttachments",
    "linkCount",
    "remoteImageCount",
    "remoteImagesLoadedCount",
    "remoteImagesLoaded",
)
DEFAULT_EMAIL_SETTINGS = {
    "mailServiceEnabled": True,
    "copyCodesEnabled": True,
    "islandCodesEnabled": True,
    "desktopNotificationsEnabled": True,
}


class GmailBridgeError(RuntimeError):
    pass


def credentials_path():
    configured = os.environ.get("ASTREA_EMAIL_GMAIL_CLIENT_SECRET", "").strip()
    return pathlib.Path(configured).expanduser() if configured else DEFAULT_CLIENT_SECRET


def load_json(path):
    with open(path, "r", encoding="utf-8") as handle:
        return json.load(handle)


def write_json(path, payload):
    path.parent.mkdir(parents=True, exist_ok=True)
    temp_path = path.with_name(path.name + ".tmp")
    with os.fdopen(os.open(temp_path, os.O_WRONLY | os.O_CREAT | os.O_TRUNC, 0o600), "w", encoding="utf-8") as handle:
        json.dump(payload, handle, ensure_ascii=False, indent=2, sort_keys=True)
    os.replace(temp_path, path)
    try:
        os.chmod(path, 0o600)
    except OSError:
        pass


def parse_bool(value):
    if isinstance(value, bool):
        return value
    text = str(value or "").strip().lower()
    if text in ("1", "true", "yes", "on", "enabled"):
        return True
    if text in ("0", "false", "no", "off", "disabled"):
        return False
    raise GmailBridgeError(f"Invalid boolean value: {value}")


def read_email_settings(path=None):
    target = pathlib.Path(path) if path is not None else EMAIL_SETTINGS_PATH
    settings = dict(DEFAULT_EMAIL_SETTINGS)
    try:
        payload = load_json(target)
    except (FileNotFoundError, json.JSONDecodeError, OSError):
        return settings
    if not isinstance(payload, dict):
        return settings
    for key in DEFAULT_EMAIL_SETTINGS:
        if key in payload:
            try:
                settings[key] = parse_bool(payload[key])
            except GmailBridgeError:
                settings[key] = DEFAULT_EMAIL_SETTINGS[key]
    return settings


def write_email_settings(settings, path=None):
    target = pathlib.Path(path) if path is not None else EMAIL_SETTINGS_PATH
    normalized = dict(DEFAULT_EMAIL_SETTINGS)
    for key in DEFAULT_EMAIL_SETTINGS:
        if key in settings:
            normalized[key] = parse_bool(settings[key])
    write_json(target, normalized)
    return normalized


def email_settings_payload(updates=None):
    settings = read_email_settings()
    changed = False
    for key, value in (updates or {}).items():
        if key not in DEFAULT_EMAIL_SETTINGS:
            raise GmailBridgeError(f"Unknown email setting: {key}")
        next_value = parse_bool(value)
        if settings.get(key) != next_value:
            settings[key] = next_value
            changed = True
    if updates is not None:
        settings = write_email_settings(settings)
    return {
        "ok": True,
        "provider": "gmail",
        "action": "settings",
        "settings": settings,
        "updated": changed,
    }


def debug_timing(action, stage, started_at, **fields):
    if not os.environ.get("ASTREA_EMAIL_DEBUG"):
        return
    elapsed_ms = (time.perf_counter() - started_at) * 1000
    details = " ".join(f"{key}={value}" for key, value in fields.items() if value is not None)
    suffix = f" {details}" if details else ""
    print(f"[astrea-email] {action}.{stage} {elapsed_ms:.1f}ms{suffix}", file=sys.stderr)


def is_qml_safe_codepoint(codepoint):
    if codepoint in (0x09, 0x0A, 0x0D):
        return True
    if codepoint < 0x20 or 0x7F <= codepoint <= 0x9F:
        return False
    if 0xFDD0 <= codepoint <= 0xFDEF:
        return False
    if codepoint >= 0xFFFE and codepoint & 0xFFFE == 0xFFFE:
        return False
    return True


def strip_qml_unsafe_chars(value):
    if value is None:
        return ""
    return "".join(ch for ch in str(value) if is_qml_safe_codepoint(ord(ch)))


def strip_unsafe_html_charrefs(value):
    def replace(match):
        raw = match.group(1)
        try:
            codepoint = int(raw[1:], 16) if raw[:1].lower() == "x" else int(raw, 10)
        except ValueError:
            return ""
        return match.group(0) if is_qml_safe_codepoint(codepoint) else ""

    return HTML_CHARREF_RE.sub(replace, value or "")


def clean_display_text(value):
    return INVISIBLE_TEXT_RE.sub("", strip_qml_unsafe_chars(strip_unsafe_html_charrefs(value or "")))


def load_client():
    path = credentials_path()
    if not path.exists():
        raise GmailBridgeError(f"Missing Gmail OAuth client: {path}")

    payload = load_json(path)
    client = payload.get("installed") or payload.get("web") or payload
    client_id = client.get("client_id", "")
    if not client_id:
        raise GmailBridgeError(f"Invalid Gmail OAuth client: {path}")

    return {
        "path": str(path),
        "client_id": client_id,
        "client_secret": client.get("client_secret", ""),
    }


def token_state(token):
    if not token:
        return "missing"
    expires_at = float(token.get("expires_at", 0) or 0)
    if token.get("access_token") and expires_at > time.time() + 60:
        return "valid"
    if token.get("refresh_token"):
        return "refreshable"
    return "expired"


def read_token():
    try:
        return load_json(TOKEN_PATH)
    except (FileNotFoundError, json.JSONDecodeError, OSError):
        return {}


def save_token(token):
    token.setdefault("scopes", SCOPES)
    write_json(TOKEN_PATH, token)


def post_form(url, payload):
    data = urllib.parse.urlencode(payload).encode("utf-8")
    request = urllib.request.Request(
        url,
        data=data,
        headers={"Content-Type": "application/x-www-form-urlencoded", "Accept": "application/json"},
        method="POST",
    )
    return open_json(request)


def open_json(request):
    try:
        with urllib.request.urlopen(request, timeout=30) as response:
            raw = response.read().decode("utf-8")
    except urllib.error.HTTPError as exc:
        body = exc.read().decode("utf-8", errors="replace")
        raise GmailBridgeError(body or str(exc)) from exc
    except urllib.error.URLError as exc:
        raise GmailBridgeError(str(exc.reason)) from exc

    if not raw:
        return {}
    try:
        return json.loads(raw)
    except json.JSONDecodeError as exc:
        raise GmailBridgeError(raw) from exc


def api_request(method, path, token, query=None, body=None):
    url = GMAIL_API + path
    if query:
        url += "?" + urllib.parse.urlencode(query, doseq=True)

    data = None
    headers = {
        "Accept": "application/json",
        "Authorization": "Bearer " + token["access_token"],
    }
    if body is not None:
        data = json.dumps(body).encode("utf-8")
        headers["Content-Type"] = "application/json"

    request = urllib.request.Request(url, data=data, headers=headers, method=method)
    return open_json(request)


def with_expiry(payload):
    token = dict(payload)
    token["expires_at"] = time.time() + int(token.get("expires_in", 3600))
    return token


def refresh_token(client, token):
    if not token.get("refresh_token"):
        raise GmailBridgeError("Gmail account is not authenticated")

    payload = {
        "client_id": client["client_id"],
        "grant_type": "refresh_token",
        "refresh_token": token["refresh_token"],
    }
    if client.get("client_secret"):
        payload["client_secret"] = client["client_secret"]

    refreshed = with_expiry(post_form(TOKEN_URL, payload))
    refreshed["refresh_token"] = token["refresh_token"]
    refreshed["account"] = token.get("account", "")
    save_token(refreshed)
    return refreshed


def ensure_token():
    client = load_client()
    token = read_token()
    state = token_state(token)
    if state == "valid":
        return token
    if state == "refreshable":
        return refresh_token(client, token)
    raise GmailBridgeError("Gmail account is not authenticated")


def code_challenge(verifier):
    digest = hashlib.sha256(verifier.encode("ascii")).digest()
    return base64.urlsafe_b64encode(digest).decode("ascii").rstrip("=")


class OAuthHandler(BaseHTTPRequestHandler):
    def do_GET(self):
        parsed = urllib.parse.urlparse(self.path)
        params = urllib.parse.parse_qs(parsed.query)
        self.server.oauth_params = {key: values[0] for key, values in params.items()}

        message = "Gmail connected. You can return to Astrea Email."
        self.send_response(200)
        self.send_header("Content-Type", "text/plain; charset=utf-8")
        self.send_header("Content-Length", str(len(message.encode("utf-8"))))
        self.end_headers()
        self.wfile.write(message.encode("utf-8"))

    def log_message(self, fmt, *args):
        return


def authenticate():
    client = load_client()
    verifier = secrets.token_urlsafe(64)
    state = secrets.token_urlsafe(24)

    with socketserver.TCPServer(("127.0.0.1", 0), OAuthHandler) as server:
        server.timeout = 180
        server.oauth_params = {}
        redirect_uri = f"http://127.0.0.1:{server.server_address[1]}/"
        params = {
            "access_type": "offline",
            "client_id": client["client_id"],
            "code_challenge": code_challenge(verifier),
            "code_challenge_method": "S256",
            "prompt": "consent",
            "redirect_uri": redirect_uri,
            "response_type": "code",
            "scope": " ".join(SCOPES),
            "state": state,
        }
        url = AUTH_URL + "?" + urllib.parse.urlencode(params)
        webbrowser.open(url)
        server.handle_request()
        result = server.oauth_params

    if result.get("state") != state:
        raise GmailBridgeError("OAuth state mismatch")
    if result.get("error"):
        raise GmailBridgeError(result["error"])
    if not result.get("code"):
        raise GmailBridgeError("OAuth flow timed out")

    token_payload = {
        "client_id": client["client_id"],
        "code": result["code"],
        "code_verifier": verifier,
        "grant_type": "authorization_code",
        "redirect_uri": redirect_uri,
    }
    if client.get("client_secret"):
        token_payload["client_secret"] = client["client_secret"]

    token = with_expiry(post_form(TOKEN_URL, token_payload))
    profile = {}
    try:
        profile = api_request("GET", "/profile", token)
    except GmailBridgeError:
        profile = {}

    token["account"] = profile.get("emailAddress", "")
    save_token(token)
    return token


def build_query(folder, message_filter, text):
    folder_terms = {
        "Inbox": "in:inbox",
        "Starred": "is:starred",
        "Sent": "in:sent",
        "Drafts": "in:drafts",
        "Archive": "-in:inbox -in:trash -in:sent -in:drafts",
        "All": "-in:trash",
        "Trash": "in:trash",
    }
    filter_terms = {
        "unread": "is:unread",
        "starred": "is:starred",
    }

    terms = [folder_terms.get(folder, "in:inbox")]
    if message_filter in filter_terms and filter_terms[message_filter] not in terms[0]:
        terms.append(filter_terms[message_filter])
    if text and text.strip():
        terms.append(text.strip())
    return " ".join(term for term in terms if term).strip()


def headers_to_dict(headers):
    mapped = {}
    for header in headers or []:
        name = clean_display_text(header.get("name", ""))
        value = clean_display_text(header.get("value", ""))
        if name:
            mapped[name] = value
    return mapped


def format_timestamp(value):
    value = clean_display_text(value)
    if not value:
        return ""
    try:
        parsed = parsedate_to_datetime(value)
        if parsed is None:
            return value
        local = parsed.astimezone()
        now = parsed.astimezone().now().astimezone()
        if local.date() == now.date():
            return local.strftime("%H:%M")
        if local.year == now.year:
            return local.strftime("%b %-d")
        return local.strftime("%b %-d, %Y")
    except (TypeError, ValueError, OSError):
        return value


def parse_headers(headers):
    raw_from = clean_display_text(headers.get("From", ""))
    from_name, from_address = parseaddr(raw_from)
    if not from_name and from_address:
        from_name = from_address.split("@")[0]
    if not from_name:
        from_name = "Unknown Sender"

    return {
        "fromName": clean_display_text(from_name),
        "fromAddress": clean_display_text(from_address),
        "subject": clean_display_text(headers.get("Subject") or "(No subject)"),
        "timestamp": format_timestamp(headers.get("Date", "")),
    }


def decode_base64url(data):
    if not data:
        return b""
    padded = data + "=" * (-len(data) % 4)
    return base64.urlsafe_b64decode(padded)


def decode_body(data):
    return clean_display_text(decode_base64url(data).decode("utf-8", errors="replace"))


def data_url(mime, data):
    if not mime or not data:
        return ""
    raw = decode_base64url(data)
    encoded = base64.b64encode(raw).decode("ascii")
    return f"data:{mime};base64,{encoded}"


def is_safe_data_image_src(value):
    match = re.match(r"(?is)^data:\s*([^;,]+)", value or "")
    if not match:
        return False
    mime = match.group(1).strip().lower()
    return mime.startswith("image/") and mime != "image/svg+xml"


def is_blocked_remote_ip(address):
    parsed = ipaddress.ip_address(address)
    return (
        parsed.is_private
        or parsed.is_loopback
        or parsed.is_link_local
        or parsed.is_multicast
        or parsed.is_reserved
        or parsed.is_unspecified
    )


def is_safe_remote_image_url(url, resolver=socket.getaddrinfo):
    parsed = urllib.parse.urlparse(url or "")
    if parsed.scheme not in ("http", "https") or not parsed.hostname:
        return False

    host = parsed.hostname.strip().lower().strip("[]")
    if host == "localhost" or host.endswith(".localhost"):
        return False

    try:
        if is_blocked_remote_ip(host):
            return False
    except ValueError:
        pass

    try:
        infos = resolver(host, parsed.port or (443 if parsed.scheme == "https" else 80), type=socket.SOCK_STREAM)
    except OSError:
        return False

    addresses = []
    for info in infos:
        sockaddr = info[4]
        if sockaddr:
            addresses.append(sockaddr[0])
    return bool(addresses) and all(not is_blocked_remote_ip(address) for address in addresses)


def remote_image_cache_root(token):
    cache_token = token or {"account": "default"}
    return CACHE_DIR / account_cache_id(cache_token) / "remote-images"


def remote_image_digest(url):
    return hashlib.sha256((url or "").encode("utf-8")).hexdigest()


def cached_remote_image_url(token, url):
    root = remote_image_cache_root(token)
    digest = remote_image_digest(url)
    for path in sorted(root.glob(digest + ".*")) if root.exists() else []:
        if path.is_file():
            return path.as_uri()
    return ""


def remote_image_extension(mime):
    lowered = (mime or "").split(";", 1)[0].strip().lower()
    return REMOTE_IMAGE_EXTENSIONS.get(lowered, ".img")


def fetch_remote_image_to_cache(url, token=None):
    cached = cached_remote_image_url(token, url)
    if cached:
        return {"url": url, "src": cached, "bytes": 0, "cached": True}

    if not is_safe_remote_image_url(url):
        return {"url": url, "src": "", "bytes": 0, "cached": False}

    request = urllib.request.Request(
        url,
        headers={
            "Accept": "image/avif,image/webp,image/apng,image/png,image/jpeg,image/gif,image/*,*/*;q=0.8",
            "User-Agent": "AstreaEmail/0.1",
        },
        method="GET",
    )
    try:
        with urllib.request.urlopen(request, timeout=REMOTE_IMAGE_TIMEOUT) as response:
            mime = (response.headers.get_content_type() or "").lower()
            if not mime.startswith("image/") or mime == "image/svg+xml":
                return {"url": url, "src": "", "bytes": 0, "cached": False}
            raw = response.read(REMOTE_IMAGE_MAX_BYTES + 1)
    except (OSError, TimeoutError, urllib.error.URLError, urllib.error.HTTPError):
        return {"url": url, "src": "", "bytes": 0, "cached": False}

    if len(raw) > REMOTE_IMAGE_MAX_BYTES:
        return {"url": url, "src": "", "bytes": 0, "cached": False}

    root = remote_image_cache_root(token)
    root.mkdir(parents=True, exist_ok=True)
    path = root / f"{remote_image_digest(url)}{remote_image_extension(mime)}"
    temp_path = path.with_name(path.name + ".tmp")
    try:
        temp_path.write_bytes(raw)
        os.replace(temp_path, path)
        os.chmod(path, 0o600)
    except OSError:
        try:
            temp_path.unlink(missing_ok=True)
        except OSError:
            pass
        return {"url": url, "src": "", "bytes": 0, "cached": False}

    return {"url": url, "src": path.as_uri(), "bytes": len(raw), "cached": False}


def extract_plain_text(payload):
    if not payload:
        return ""
    mime = payload.get("mimeType", "")
    body = payload.get("body", {})
    if mime.startswith("text/plain") and body.get("data"):
        return decode_body(body["data"])

    for part in payload.get("parts", []) or []:
        extracted = extract_plain_text(part)
        if extracted:
            return extracted

    if body.get("data") and not mime.startswith("text/html"):
        return decode_body(body["data"])
    return ""


def extract_html_body(payload, attachment_loader=None):
    if not payload:
        return ""
    mime = payload.get("mimeType", "")
    body = payload.get("body", {})
    if mime.startswith("text/html") and body.get("data"):
        return decode_body(body["data"])
    if mime.startswith("text/html") and body.get("attachmentId") and attachment_loader is not None:
        data = attachment_loader(body["attachmentId"])
        if data:
            return decode_body(data)

    for part in payload.get("parts", []) or []:
        extracted = extract_html_body(part, attachment_loader)
        if extracted:
            return extracted

    return ""


class EmailHtmlSanitizer(HTMLParser):
    block_tags = {"script", "style", "head", "meta", "link", "iframe", "object", "embed", "form", "input", "button", "textarea", "select", "option", "canvas", "svg"}
    drop_tags = {"html", "body"}
    safe_attrs = {
        "abbr", "align", "alt", "bgcolor", "border", "cellpadding", "cellspacing",
        "colspan", "dir", "height", "href", "hspace", "lang", "rowspan", "src",
        "style", "target", "title", "valign", "vspace", "width",
    }
    void_tags = {"area", "br", "hr", "img", "input", "meta", "link"}

    def __init__(self, cid_sources=None, remote_image_loader=None):
        super().__init__(convert_charrefs=False)
        self.parts = []
        self.cid_sources = cid_sources or {}
        self.remote_image_loader = remote_image_loader
        self.remote_image_count = 0
        self.remote_images_loaded = 0
        self.skip_stack = []

    def should_skip(self):
        return len(self.skip_stack) > 0

    def handle_starttag(self, tag, attrs):
        tag = tag.lower()
        if tag in self.block_tags:
            self.skip_stack.append(tag)
            return
        if self.should_skip():
            return
        if tag in self.drop_tags:
            return

        if tag == "img" and self.is_tracking_image(attrs):
            return

        attrs_text = self.clean_attrs(tag, attrs)
        if tag == "img" and 'src="' not in attrs_text:
            return
        if tag == "img":
            self.parts.append(f'<p align="center"><img{attrs_text} /></p>')
            return

        suffix = " /" if tag in self.void_tags else ""
        self.parts.append(f"<{tag}{attrs_text}{suffix}>")

    def handle_startendtag(self, tag, attrs):
        tag = tag.lower()
        if tag in self.block_tags or self.should_skip() or tag in self.drop_tags:
            return
        attrs_text = self.clean_attrs(tag, attrs)
        if tag == "img":
            if self.is_tracking_image(attrs) or 'src="' not in attrs_text:
                return
            self.parts.append(f'<p align="center"><img{attrs_text} /></p>')
            return
        self.parts.append(f"<{tag}{attrs_text} />")

    def handle_endtag(self, tag):
        tag = tag.lower()
        if self.skip_stack:
            if tag == self.skip_stack[-1]:
                self.skip_stack.pop()
            return
        if tag in self.drop_tags or tag in self.block_tags or tag in self.void_tags:
            return
        self.parts.append(f"</{tag}>")

    def handle_data(self, data):
        if not self.should_skip():
            self.parts.append(html.escape(clean_display_text(data), quote=False))

    def handle_entityref(self, name):
        if not self.should_skip():
            resolved = clean_display_text(html.unescape(f"&{name};"))
            if resolved:
                self.parts.append(f"&{name};")

    def handle_charref(self, name):
        if not self.should_skip():
            try:
                codepoint = int(name[1:], 16) if name[:1].lower() == "x" else int(name, 10)
            except ValueError:
                return
            if is_qml_safe_codepoint(codepoint):
                self.parts.append(f"&#{name};")

    def clean_attrs(self, tag, attrs):
        cleaned = []
        for name, value in attrs:
            attr = (name or "").lower()
            if attr.startswith("on") or attr not in self.safe_attrs:
                continue
            if value is None:
                value = ""
            value = self.clean_attr_value(attr, value)
            if value == "":
                continue
            if tag == "img" and attr in ("width", "height"):
                value = self.clamped_image_dimension(attr, value)
                if value == "":
                    continue
            if attr == "style":
                value = self.clean_style_value(value)
                if value == "":
                    continue
            cleaned.append(f'{attr}="{html.escape(value, quote=True)}"')
        return (" " + " ".join(cleaned)) if cleaned else ""

    def clean_attr_value(self, attr, value):
        stripped = clean_display_text(value or "").strip()
        lowered = stripped.lower()
        if attr in ("href", "src"):
            if attr == "href":
                if stripped.startswith("//"):
                    stripped = "https:" + stripped
                    lowered = stripped.lower()
                parsed = urllib.parse.urlparse(stripped)
                if parsed.scheme in ("http", "https"):
                    return stripped if parsed.netloc else ""
                if parsed.scheme == "mailto":
                    return stripped
                return ""
            if lowered.startswith("cid:"):
                cid = urllib.parse.unquote(stripped[4:]).strip("<>")
                return self.cid_sources.get(cid, "")
            if lowered.startswith("data:"):
                return stripped if is_safe_data_image_src(stripped) else ""
            if lowered.startswith(("http://", "https://")):
                self.remote_image_count += 1
                if self.remote_image_loader is None:
                    return ""
                loaded = self.remote_image_loader(stripped) or ""
                if loaded:
                    self.remote_images_loaded += 1
                return loaded
            if stripped.startswith("//"):
                self.remote_image_count += 1
                if self.remote_image_loader is None:
                    return ""
                loaded = self.remote_image_loader("https:" + stripped) or ""
                if loaded:
                    self.remote_images_loaded += 1
                return loaded
            return ""
        return stripped

    def clean_style_value(self, value):
        cleaned = re.sub(r"(?is)url\s*\([^)]*\)", "", value or "")
        cleaned = re.sub(r"(?is)expression\s*\([^)]*\)", "", cleaned)
        return cleaned.strip()

    def clamped_image_dimension(self, attr, value):
        try:
            number = int(float(str(value).strip().rstrip("px")))
        except (TypeError, ValueError):
            return ""
        if number <= 0:
            return ""
        if attr == "width":
            number = min(number, HTML_IMAGE_MAX_WIDTH)
        return str(number)

    def is_tracking_image(self, attrs):
        mapped = {str(name or "").lower(): str(value or "").strip() for name, value in attrs or []}
        src = mapped.get("src", "").lower()
        try:
            width = int(float(mapped.get("width", "0").rstrip("px") or 0))
            height = int(float(mapped.get("height", "0").rstrip("px") or 0))
        except ValueError:
            width = 0
            height = 0
        if width and height and width <= 2 and height <= 2:
            return True
        return "pixel." in src or "tracking" in src or "/open" in src

    def html(self):
        return "".join(self.parts).strip()


class EmailTextExtractor(HTMLParser):
    block_tags = {
        "address", "article", "aside", "blockquote", "br", "caption", "div",
        "footer", "h1", "h2", "h3", "h4", "h5", "h6", "header", "hr", "li",
        "main", "p", "section", "table", "tbody", "td", "tfoot", "th", "thead",
        "tr", "ul", "ol",
    }

    def __init__(self):
        super().__init__(convert_charrefs=True)
        self.parts = []
        self.skip_stack = []

    def should_skip(self):
        return len(self.skip_stack) > 0

    def push_break(self):
        if self.parts and self.parts[-1] != "\n":
            self.parts.append("\n")

    def handle_starttag(self, tag, attrs):
        tag = tag.lower()
        if tag in EmailHtmlSanitizer.block_tags:
            self.skip_stack.append(tag)
            return
        if self.should_skip():
            return
        if tag in self.block_tags:
            self.push_break()

    def handle_startendtag(self, tag, attrs):
        tag = tag.lower()
        if not self.should_skip() and tag in self.block_tags:
            self.push_break()

    def handle_endtag(self, tag):
        tag = tag.lower()
        if self.skip_stack:
            if tag == self.skip_stack[-1]:
                self.skip_stack.pop()
            return
        if tag in self.block_tags:
            self.push_break()

    def handle_data(self, data):
        if not self.should_skip() and data:
            self.parts.append(clean_display_text(data))

    def text(self):
        return clean_reader_text("".join(self.parts))


def normalize_external_link(url):
    stripped = clean_display_text(url or "").strip()
    if not stripped:
        return ""
    if stripped.startswith("//"):
        stripped = "https:" + stripped

    parsed = urllib.parse.urlparse(stripped)
    if parsed.scheme in ("http", "https"):
        return stripped if parsed.netloc else ""
    if parsed.scheme == "mailto":
        return stripped if parsed.path else ""
    return ""


def is_safe_external_link(url):
    return normalize_external_link(url) != ""


class EmailLinkExtractor(HTMLParser):
    def __init__(self, limit=LINK_EXTRACT_LIMIT):
        super().__init__(convert_charrefs=True)
        self.limit = int(limit or LINK_EXTRACT_LIMIT)
        self.links = []
        self.link_stack = []
        self.seen = set()
        self.skip_stack = []

    def should_skip(self):
        return len(self.skip_stack) > 0

    def handle_starttag(self, tag, attrs):
        tag = tag.lower()
        if tag in EmailHtmlSanitizer.block_tags:
            self.skip_stack.append(tag)
            return
        if self.should_skip():
            return
        if tag != "a":
            return

        mapped = {str(name or "").lower(): value for name, value in attrs or []}
        url = normalize_external_link(mapped.get("href", ""))
        if not url or len(self.links) >= self.limit:
            return
        self.link_stack.append({"url": url, "parts": []})

    def handle_endtag(self, tag):
        tag = tag.lower()
        if self.skip_stack:
            if tag == self.skip_stack[-1]:
                self.skip_stack.pop()
            return
        if tag != "a" or not self.link_stack:
            return

        link = self.link_stack.pop()
        url = link["url"]
        if url in self.seen:
            return
        label = clean_text("".join(link["parts"]), LINK_LABEL_LIMIT)
        if not label:
            label = clean_text(url, LINK_LABEL_LIMIT)
        self.seen.add(url)
        self.links.append({"url": url, "label": label})

    def handle_data(self, data):
        if not self.should_skip() and self.link_stack and data:
            self.link_stack[-1]["parts"].append(clean_display_text(data))


def extract_email_links(raw_html, limit=LINK_EXTRACT_LIMIT):
    if not raw_html:
        return []
    parser = EmailLinkExtractor(limit=limit)
    parser.feed(strip_block_html(clean_display_text(raw_html)))
    parser.close()
    return parser.links


def sanitize_html_email(raw_html, attachments, remote_image_loader=None):
    return sanitize_html_email_details(raw_html, attachments, remote_image_loader)["html"]


def sanitize_html_email_details(raw_html, attachments, remote_image_loader=None, force_html=False):
    if not raw_html:
        return html_details_payload("", 0, 0, 0, "", "plain", False, 0)
    prepared_html = html_body_fragment(strip_block_html(clean_display_text(raw_html)))
    table_count = count_html_tables(prepared_html)
    if not force_html and should_use_reader_mode(prepared_html, table_count):
        if remote_image_loader is not None:
            original_details = sanitize_prepared_html_email_details(prepared_html, attachments, remote_image_loader)
            if original_details["remoteImagesLoadedCount"] > 0:
                return original_details

        remote_details = sanitize_prepared_html_email_details(prepared_html, attachments, None)
        reader_body = html_to_reader_text(prepared_html)
        return html_details_payload(
            reader_text_to_html(reader_body),
            remote_details["remoteImageCount"],
            remote_details["remoteImagesLoadedCount"],
            len(prepared_html),
            reader_body,
            "reader",
            True,
            table_count,
        )

    return sanitize_prepared_html_email_details(prepared_html, attachments, remote_image_loader)


def sanitize_prepared_html_email_details(prepared_html, attachments, remote_image_loader=None):
    cid_sources = {}
    for attachment in attachments or []:
        content_id = attachment.get("contentId", "")
        data_url_value = attachment.get("dataUrl", "")
        if content_id and data_url_value:
            cid_sources[content_id] = data_url_value

    parser = EmailHtmlSanitizer(cid_sources, remote_image_loader)
    parser.feed(prepared_html)
    parser.close()
    html_value = centered_html(parser.html())
    return html_details_payload(
        html_value,
        parser.remote_image_count,
        parser.remote_images_loaded,
        len(html_value),
        "",
        "html" if html_value else "plain",
        False,
        count_html_tables(html_value),
    )


def html_details_payload(html_value, remote_image_count, remote_images_loaded, html_length, reader_body, render_mode, suppressed, table_count):
    return {
        "html": html_value,
        "readerBody": reader_body,
        "htmlRenderMode": render_mode,
        "htmlSuppressed": suppressed,
        "htmlLength": html_length,
        "htmlTableCount": table_count,
        "remoteImageCount": remote_image_count,
        "remoteImagesLoadedCount": remote_images_loaded,
    }


def count_html_tables(value):
    return len(re.findall(r"(?is)<table\b", value or ""))


def should_use_reader_mode(value, table_count=None):
    html_value = value or ""
    tables = count_html_tables(html_value) if table_count is None else table_count
    return (
        len(html_value) > HTML_RENDER_CHAR_LIMIT
        or tables > HTML_RENDER_TABLE_LIMIT
        or has_complex_email_layout(html_value, tables)
    )


def has_complex_email_layout(value, table_count=None):
    html_value = value or ""
    tables = count_html_tables(html_value) if table_count is None else table_count
    if tables >= HTML_COMPLEX_TABLE_LIMIT:
        return True
    if tables >= 3 and len(html_value) >= HTML_COMPLEX_LAYOUT_CHAR_LIMIT:
        return True
    if tables >= 4 and re.search(r"(?is)\b(?:pix|pagamento|payment|invoice|receipt|order)\b", html_value):
        return True

    markers = 0
    markers += len(re.findall(r"(?is)<t[dh]\b", html_value))
    markers += len(re.findall(r"(?is)\b(?:bgcolor|background|valign|cellpadding|cellspacing)\s*=", html_value))
    markers += len(re.findall(r"(?is)\b(?:width|height)\s*=\s*[\"']?\d{3,}", html_value))
    markers += len(re.findall(r"(?is)\b(?:mso-|max-width|min-width|font-family|line-height)\b", html_value))
    return tables >= 6 and markers >= 18


def has_escaped_reader_markup(value):
    return bool(re.search(r"(?is)&lt;\s*/?\s*(?:ul|ol|li)\b", value or ""))


def centered_html(value):
    html_value = clean_display_text(value or "").strip()
    if not html_value:
        return ""
    if re.match(r'(?is)^<div\s+align=["\']center["\']', html_value):
        return html_value
    return f'<div align="center">{html_value}</div>'


def html_to_reader_text(raw_html):
    if not raw_html:
        return ""
    parser = EmailTextExtractor()
    parser.feed(raw_html)
    parser.close()
    text = parser.text()
    if len(text) > HTML_READER_BODY_LIMIT:
        return text[: HTML_READER_BODY_LIMIT - 1].rstrip() + "..."
    return text


def clean_reader_text(value):
    text = html.unescape(clean_display_text(value or ""))
    text = INVISIBLE_TEXT_RE.sub("", text)
    raw_lines = text.replace("\r", "\n").splitlines()
    lines = []
    seen = set()
    previous_blank = False
    for raw_line in raw_lines:
        line = " ".join(raw_line.split()).strip()
        line = URL_TOKEN_RE.sub("", line)
        line = re.sub(r"\s+([,.;:!?])", r"\1", line)
        line = re.sub(r"\s{2,}", " ", line).strip(" \t|")
        if not line or re.fullmatch(r"[-–—_.,;:|/\\ ]+", line):
            if lines and not previous_blank:
                lines.append("")
            previous_blank = True
            continue
        lowered = line.lower()
        if lowered in seen:
            continue
        seen.add(lowered)
        lines.append(line)
        previous_blank = False
    return "\n".join(lines).strip()


def reader_text_to_html(value):
    text = clean_reader_text(value)
    if not text:
        return ""
    parts = ['<div align="left">']
    in_list = False
    for line in text.splitlines():
        raw_line = html.unescape(clean_display_text(line)).strip()
        lowered = raw_line.lower()
        if re.fullmatch(r"<(?:ul|ol)\b[^>]*>", lowered):
            if not in_list:
                parts.append("<ul>")
                in_list = True
            continue
        if re.fullmatch(r"</(?:ul|ol)\s*>", lowered):
            if in_list:
                parts.append("</ul>")
                in_list = False
            continue
        item_match = re.fullmatch(r"(?is)<li\b[^>]*>(.*?)</li\s*>", raw_line)
        if item_match:
            if not in_list:
                parts.append("<ul>")
                in_list = True
            item = clean_reader_text(item_match.group(1))
            if item:
                parts.append(f"<li>{html.escape(item, quote=False)}</li>")
            continue

        if not raw_line:
            if in_list:
                continue
            parts.append("<br />")
            continue
        if in_list:
            parts.append("</ul>")
            in_list = False
        parts.append(f"<p>{html.escape(raw_line, quote=False)}</p>")
    if in_list:
        parts.append("</ul>")
    parts.append("</div>")
    return "".join(parts)


def html_body_fragment(raw_html):
    match = re.search(r"(?is)<body\b[^>]*>", raw_html or "")
    if not match:
        return raw_html or ""
    fragment = raw_html[match.end():]
    end = re.search(r"(?is)</body\s*>", fragment)
    if end:
        fragment = fragment[: end.start()]
    return fragment


def strip_block_html(raw_html):
    value = clean_display_text(raw_html or "")
    for tag in ("script", "style", "head", "iframe", "object", "embed", "form", "svg"):
        value = re.sub(rf"(?is)<{tag}\b[^>]*>.*?</{tag}\s*>", "", value)
    value = re.sub(r"(?is)<!--.*?-->", "", value)
    return value


def clean_text(value, limit=0):
    text = html.unescape(clean_display_text(value or ""))
    text = " ".join(text.replace("\r", "\n").split())
    if limit and len(text) > limit:
        return text[: limit - 1].rstrip() + "..."
    return text


def account_cache_id(token):
    account = (token.get("account") or "default").strip().lower()
    digest = hashlib.sha256(account.encode("utf-8")).hexdigest()
    return digest[:24]


def account_cache_root(token):
    return CACHE_DIR / account_cache_id(token)


def message_summary_for_list(message):
    summary = {}
    for key in SUMMARY_MESSAGE_FIELDS:
        if key in message:
            summary[key] = message[key]

    message_id = summary.get("messageId") or message.get("id", "")
    folder = summary.get("folder") or "Inbox"
    summary.update({
        "messageId": message_id,
        "threadId": summary.get("threadId", ""),
        "historyId": summary.get("historyId", ""),
        "folder": folder,
        "fromName": clean_display_text(summary.get("fromName", "")) or "Unknown Sender",
        "fromAddress": clean_display_text(summary.get("fromAddress", "")),
        "subject": clean_display_text(summary.get("subject", "")) or "(No subject)",
        "preview": clean_text(summary.get("preview", ""), 140) or "No preview available",
        "timestamp": clean_display_text(summary.get("timestamp", "")),
        "tag": summary.get("tag") or ("Gmail" if folder == "Inbox" else folder),
        "starred": bool(summary.get("starred", False)),
        "isRead": bool(summary.get("isRead", True)),
        "importance": summary.get("importance") or "normal",
        "hasAttachments": bool(summary.get("hasAttachments", False)),
        "linkCount": int(summary.get("linkCount", 0) or 0),
        "remoteImageCount": int(summary.get("remoteImageCount", 0) or 0),
        "remoteImagesLoadedCount": int(summary.get("remoteImagesLoadedCount", 0) or 0),
        "remoteImagesLoaded": bool(summary.get("remoteImagesLoaded", False)),
        "detailLoaded": False,
    })
    return summary


def summary_patch_for_list(patch):
    summary = {}
    for key in SUMMARY_MESSAGE_FIELDS:
        if key in patch:
            summary[key] = patch[key]
    if "messageId" in patch:
        summary["messageId"] = patch["messageId"]
    if summary:
        summary["detailLoaded"] = False
    return summary


def summary_index_path(token):
    return account_cache_root(token) / SUMMARY_INDEX_FILE


def read_summary_index(token):
    try:
        payload = load_json(summary_index_path(token))
    except (FileNotFoundError, json.JSONDecodeError, OSError):
        return {}
    if payload.get("cacheVersion") != CACHE_VERSION or payload.get("kind") != "summaryIndex":
        return {}
    messages = payload.get("messages", {})
    if not isinstance(messages, dict):
        return {}
    return {
        str(message_id): message_summary_for_list(message)
        for message_id, message in messages.items()
        if isinstance(message, dict) and message_id
    }


def write_summary_index(token, messages):
    normalized = {
        str(message_id): message_summary_for_list(message)
        for message_id, message in messages.items()
        if message_id and isinstance(message, dict)
    }
    try:
        write_json(summary_index_path(token), {
            "cacheVersion": CACHE_VERSION,
            "kind": "summaryIndex",
            "updatedAt": time.time(),
            "messages": normalized,
        })
    except OSError:
        pass


def remember_message_summary(token, message):
    summary = message_summary_for_list(message)
    message_id = summary.get("messageId", "")
    if not message_id:
        return
    messages = read_summary_index(token)
    existing = dict(messages.get(message_id, {}))
    existing.update(summary)
    messages[message_id] = existing
    write_summary_index(token, messages)


def remember_message_summaries(token, messages):
    indexed = read_summary_index(token)
    changed = False
    for message in messages or []:
        summary = message_summary_for_list(message)
        message_id = summary.get("messageId", "")
        if not message_id:
            continue
        existing = dict(indexed.get(message_id, {}))
        existing.update(summary)
        indexed[message_id] = existing
        changed = True
    if changed:
        write_summary_index(token, indexed)


def update_summary_index_patch(token, message_id, patch):
    if not message_id:
        return
    summary_patch = summary_patch_for_list(patch)
    if not summary_patch:
        return
    messages = read_summary_index(token)
    existing = dict(messages.get(message_id, {"messageId": message_id}))
    existing.update(summary_patch)
    messages[message_id] = message_summary_for_list(existing)
    write_summary_index(token, messages)


def message_detail_cache_path(token, message_id):
    digest = hashlib.sha256(str(message_id).encode("utf-8")).hexdigest()
    return account_cache_root(token) / DETAIL_CACHE_DIR / f"{digest}.json"


def read_message_detail_cache(token, message_id, load_remote_images=False, force_html=False):
    if load_remote_images:
        return None
    try:
        payload = load_json(message_detail_cache_path(token, message_id))
    except (FileNotFoundError, json.JSONDecodeError, OSError):
        return None
    if payload.get("cacheVersion") != CACHE_VERSION or payload.get("kind") != "messageDetail":
        return None
    message = payload.get("message")
    if not isinstance(message, dict) or not message.get("detailLoaded"):
        return None
    if force_html and message.get("htmlRenderMode") != "html":
        return None
    return message


def write_message_detail_cache(token, message):
    if not message.get("messageId"):
        return
    try:
        write_json(message_detail_cache_path(token, message["messageId"]), {
            "cacheVersion": CACHE_VERSION,
            "kind": "messageDetail",
            "updatedAt": time.time(),
            "message": message,
        })
    except OSError:
        pass


def cache_key(folder, message_filter, query, limit, page_token):
    payload = {
        "folder": folder,
        "filter": message_filter,
        "query": query or "",
        "pageToken": page_token or "",
    }
    raw = json.dumps(payload, ensure_ascii=False, sort_keys=True, separators=(",", ":"))
    return hashlib.sha256(raw.encode("utf-8")).hexdigest()


def cache_path(token, folder, message_filter, query, limit, page_token):
    return account_cache_root(token) / f"{cache_key(folder, message_filter, query, limit, page_token)}.json"


def read_cache(token, folder, message_filter, query, limit, page_token):
    exact_path = cache_path(token, folder, message_filter, query, limit, page_token)

    def read_candidate(path):
        try:
            payload = load_json(path)
        except (FileNotFoundError, json.JSONDecodeError, OSError):
            return None

        if payload.get("provider") != "gmail" or not isinstance(payload.get("messages"), list):
            return None
        if payload.get("folder") != folder or payload.get("filter") != message_filter:
            return None
        if (payload.get("query") or "") != (query or ""):
            return None
        if (payload.get("pageToken") or "") != (page_token or ""):
            return None
        try:
            cache_version = int(payload.get("cacheVersion", 0) or 0)
        except (TypeError, ValueError):
            cache_version = 0
        if cache_version != CACHE_VERSION:
            return None

        if upgrade_cached_payload(payload):
            payload["cachedAt"] = time.time()
            try:
                write_json(path, payload)
            except OSError:
                pass

        payload["messages"] = [message_summary_for_list(message) for message in payload.get("messages", [])]
        payload["cached"] = True
        payload["cachePath"] = str(path)
        payload["cacheStale"] = False
        return payload

    exact_payload = read_candidate(exact_path)
    if exact_payload is not None:
        return exact_payload

    cache_root = account_cache_root(token)
    if not cache_root.exists():
        return None

    for path in sorted(cache_root.glob("*.json"), key=lambda candidate: candidate.stat().st_mtime, reverse=True):
        if path == exact_path:
            continue
        payload = read_candidate(path)
        if payload is not None:
            return payload

    return None


def write_cache(token, folder, message_filter, query, limit, page_token, payload):
    path = cache_path(token, folder, message_filter, query, limit, page_token)
    cached_payload = dict(payload)
    cached_payload["messages"] = [message_summary_for_list(message) for message in payload.get("messages", [])]
    cached_payload["cacheVersion"] = CACHE_VERSION
    cached_payload["cachedAt"] = time.time()
    cached_payload["cached"] = False
    cached_payload["cacheStale"] = False
    cached_payload["cachePath"] = str(path)
    write_json(path, cached_payload)


def update_cached_message(token, message):
    summary = message_summary_for_list(message)
    message_id = summary.get("messageId", "")
    if not message_id:
        return
    remember_message_summary(token, summary)

    cache_root = account_cache_root(token)
    if not cache_root.exists():
        return

    for path in cache_root.glob("*.json"):
        try:
            payload = load_json(path)
        except (json.JSONDecodeError, OSError):
            continue

        messages = payload.get("messages", [])
        if not isinstance(messages, list):
            continue
        changed = False
        for index, cached_message in enumerate(messages):
            if cached_message.get("messageId") == message_id:
                messages[index] = summary
                changed = True

        if changed:
            payload["messages"] = messages
            payload["cachedAt"] = time.time()
            write_json(path, payload)


def update_cached_message_patch(token, message_id, patch):
    update_summary_index_patch(token, message_id, patch)
    cache_root = account_cache_root(token)
    if not cache_root.exists() or not message_id:
        return

    summary_patch = summary_patch_for_list(patch)
    for path in cache_root.glob("*.json"):
        try:
            payload = load_json(path)
        except (json.JSONDecodeError, OSError):
            continue

        messages = payload.get("messages", [])
        if not isinstance(messages, list):
            continue
        changed = False
        for index, cached_message in enumerate(messages):
            if cached_message.get("messageId") == message_id:
                updated = dict(cached_message)
                updated.update(summary_patch)
                messages[index] = message_summary_for_list(updated)
                changed = True

        if changed:
            payload["messages"] = messages
            payload["cachedAt"] = time.time()
            write_json(path, payload)


def upgrade_cached_payload(payload):
    changed = False
    messages = payload.get("messages", [])
    for index, cached_message in enumerate(messages):
        upgraded, message_changed = upgrade_cached_message(cached_message)
        if message_changed:
            messages[index] = upgraded
            changed = True
    if changed:
        payload["messages"] = messages
    return changed


def upgrade_cached_message(message):
    if not isinstance(message, dict):
        return message, False

    upgraded = dict(message)
    changed = False
    for key in ("fromName", "fromAddress", "subject", "preview", "body", "htmlBody", "timestamp", "tag"):
        if isinstance(upgraded.get(key), str):
            cleaned = clean_text(upgraded.get(key), 140) if key == "preview" else clean_display_text(upgraded.get(key))
            if cleaned != upgraded.get(key):
                upgraded[key] = cleaned
                changed = True
    html_value = upgraded.get("htmlBody") or ""
    table_count = int(upgraded.get("htmlTableCount") or count_html_tables(html_value) or 0)

    if upgraded.get("htmlRenderMode") == "reader" or has_escaped_reader_markup(html_value):
        reader_source = upgraded.get("body") or upgraded.get("preview") or ""
        if html_value:
            extracted = html_to_reader_text(html_value)
            if len(extracted) > len(clean_reader_text(reader_source)):
                reader_source = extracted
        reader_body = clean_reader_text(reader_source)
        reader_html = reader_text_to_html(reader_body)
        if upgraded.get("body") != reader_body and reader_body:
            upgraded["body"] = reader_body
            changed = True
        if upgraded.get("htmlBody") != reader_html:
            upgraded["htmlBody"] = reader_html
            changed = True
        if upgraded.get("htmlRenderMode") != "reader":
            upgraded["htmlRenderMode"] = "reader"
            changed = True
        if upgraded.get("htmlSuppressed") is not True:
            upgraded["htmlSuppressed"] = True
            changed = True
        html_length = int(upgraded.get("htmlLength") or len(html_value) or 0)
        if upgraded.get("htmlLength") != html_length:
            upgraded["htmlLength"] = html_length
            changed = True
        if upgraded.get("htmlTableCount") != table_count:
            upgraded["htmlTableCount"] = table_count
            changed = True
        if upgraded.get("remoteImageCount", 0) != 0:
            upgraded["remoteImageCount"] = 0
            changed = True
        if upgraded.get("remoteImagesLoadedCount", 0) != 0:
            upgraded["remoteImagesLoadedCount"] = 0
            changed = True
        if upgraded.get("remoteImagesLoaded") is not False:
            upgraded["remoteImagesLoaded"] = False
            changed = True
    elif html_value and should_use_reader_mode(html_value, table_count):
        reader_body = html_to_reader_text(html_value)
        body_value = upgraded.get("body") or upgraded.get("preview") or ""
        body_length = len(clean_text(body_value))
        if reader_body and body_length < HTML_READER_BODY_MIN_CHARS and len(reader_body) > body_length:
            upgraded["body"] = reader_body
        upgraded["htmlBody"] = reader_text_to_html(reader_body)
        upgraded["htmlRenderMode"] = "reader"
        upgraded["htmlSuppressed"] = True
        upgraded["htmlLength"] = len(html_value)
        upgraded["htmlTableCount"] = table_count
        upgraded["remoteImageCount"] = 0
        upgraded["remoteImagesLoadedCount"] = 0
        upgraded["remoteImagesLoaded"] = False
        changed = True
    elif html_value:
        centered = centered_html(html_value)
        if centered != html_value:
            upgraded["htmlBody"] = centered
            changed = True
        if upgraded.get("htmlRenderMode") != "html":
            upgraded["htmlRenderMode"] = "html"
            changed = True
        if bool(upgraded.get("htmlSuppressed")):
            upgraded["htmlSuppressed"] = False
            changed = True
        html_length = len(centered)
        if upgraded.get("htmlLength") != html_length:
            upgraded["htmlLength"] = html_length
            changed = True
        if upgraded.get("htmlTableCount") != count_html_tables(centered):
            upgraded["htmlTableCount"] = count_html_tables(centered)
            changed = True
    else:
        defaults = {
            "htmlRenderMode": upgraded.get("htmlRenderMode") or "plain",
            "htmlSuppressed": bool(upgraded.get("htmlSuppressed", False)),
            "htmlLength": int(upgraded.get("htmlLength") or 0),
            "htmlTableCount": int(upgraded.get("htmlTableCount") or 0),
        }
        for key, value in defaults.items():
            if upgraded.get(key) != value:
                upgraded[key] = value
                changed = True

    return upgraded, changed


def header_value(headers, name):
    wanted = name.lower()
    for header_name, value in (headers or {}).items():
        if header_name.lower() == wanted:
            return value
    return ""


def attachment_label(filename, mime, content_id):
    if filename:
        return filename
    if content_id:
        return content_id.strip("<>")
    if mime.startswith("image/"):
        return "Inline image"
    return "Attachment"


def extract_attachments(payload, attachment_loader=None):
    attachments = []

    def walk(part):
        if not part:
            return

        mime = part.get("mimeType", "")
        if mime.startswith("multipart/"):
            for child in part.get("parts", []) or []:
                walk(child)
            return

        body = part.get("body", {}) or {}
        headers = headers_to_dict(part.get("headers", []))
        filename = (part.get("filename") or "").strip()
        attachment_id = body.get("attachmentId", "")
        size = int(body.get("size", 0) or 0)
        content_id = header_value(headers, "Content-ID").strip()
        disposition = header_value(headers, "Content-Disposition").strip()
        disposition_lower = disposition.lower()
        is_image = mime.startswith("image/")
        is_text_body = mime.startswith("text/plain") or mime.startswith("text/html")
        is_explicit_attachment = bool(filename or disposition_lower.startswith("attachment"))
        is_attachment = bool(
            is_explicit_attachment
            or (attachment_id and not is_text_body)
            or (is_image and (body.get("data") or content_id))
        )

        if is_attachment:
            data = body.get("data", "")
            if (
                not data
                and attachment_id
                and attachment_loader is not None
                and is_image
                and size <= INLINE_IMAGE_LIMIT_BYTES
            ):
                data = attachment_loader(attachment_id) or ""

            attachments.append({
                "id": attachment_id or content_id.strip("<>") or f"part-{len(attachments) + 1}",
                "name": attachment_label(filename, mime, content_id),
                "mimeType": mime or "application/octet-stream",
                "size": size,
                "inline": disposition_lower.startswith("inline") or bool(content_id and not filename),
                "contentId": content_id.strip("<>"),
                "dataUrl": data_url(mime, data) if is_image and data else "",
            })

        for child in part.get("parts", []) or []:
            walk(child)

    walk(payload)
    return attachments


def folder_from_labels(labels):
    label_set = set(labels or [])
    if "TRASH" in label_set:
        return "Trash"
    if "SENT" in label_set:
        return "Sent"
    if "DRAFT" in label_set:
        return "Drafts"
    if "INBOX" in label_set:
        return "Inbox"
    return "Archive"


def normalize_message(message, attachment_loader=None, remote_image_loader=None, force_html=False):
    labels = message.get("labelIds", [])
    payload = message.get("payload", {})
    headers = parse_headers(headers_to_dict(payload.get("headers", [])))
    body = extract_plain_text(payload).strip()
    attachments = extract_attachments(payload, attachment_loader)
    raw_html = extract_html_body(payload, attachment_loader)
    links = extract_email_links(raw_html)
    html_details = sanitize_html_email_details(
        raw_html,
        attachments,
        remote_image_loader,
        force_html=force_html,
    )
    body_for_display = body
    if (
        html_details["htmlSuppressed"]
        and html_details["readerBody"]
        and len(clean_text(body_for_display)) < HTML_READER_BODY_MIN_CHARS
        and len(html_details["readerBody"]) > len(clean_text(body_for_display))
    ):
        body_for_display = html_details["readerBody"]
    snippet = clean_text(message.get("snippet", ""), 140)
    folder = folder_from_labels(labels)

    return {
        "messageId": message.get("id", ""),
        "threadId": message.get("threadId", ""),
        "historyId": message.get("historyId", ""),
        "folder": folder,
        "fromName": headers["fromName"],
        "fromAddress": headers["fromAddress"],
        "subject": headers["subject"],
        "preview": snippet or clean_text(body_for_display, 140) or "No preview available",
        "body": body_for_display or snippet or "No plain text body available.",
        "htmlBody": html_details["html"],
        "htmlRenderMode": html_details["htmlRenderMode"],
        "htmlSuppressed": html_details["htmlSuppressed"],
        "htmlLength": html_details["htmlLength"],
        "htmlTableCount": html_details["htmlTableCount"],
        "links": links,
        "linkCount": len(links),
        "timestamp": headers["timestamp"],
        "tag": "Gmail" if folder == "Inbox" else folder,
        "starred": "STARRED" in labels,
        "isRead": "UNREAD" not in labels,
        "importance": "high" if "IMPORTANT" in labels else "normal",
        "attachments": attachments,
        "hasAttachments": len(attachments) > 0,
        "remoteImageCount": html_details["remoteImageCount"],
        "remoteImagesLoadedCount": html_details["remoteImagesLoadedCount"],
        "remoteImagesLoaded": html_details["remoteImagesLoadedCount"] > 0,
        "detailLoaded": True,
    }


def normalize_message_summary(message):
    labels = message.get("labelIds", [])
    payload = message.get("payload", {})
    headers = parse_headers(headers_to_dict(payload.get("headers", [])))
    folder = folder_from_labels(labels)
    return {
        "messageId": message.get("id", ""),
        "threadId": message.get("threadId", ""),
        "historyId": message.get("historyId", ""),
        "folder": folder,
        "fromName": headers["fromName"],
        "fromAddress": headers["fromAddress"],
        "subject": headers["subject"],
        "preview": clean_text(message.get("snippet", ""), 140) or "No preview available",
        "timestamp": headers["timestamp"],
        "tag": "Gmail" if folder == "Inbox" else folder,
        "starred": "STARRED" in labels,
        "isRead": "UNREAD" not in labels,
        "importance": "high" if "IMPORTANT" in labels else "normal",
        "hasAttachments": "HAS_ATTACHMENT" in labels,
        "linkCount": 0,
        "remoteImageCount": 0,
        "remoteImagesLoadedCount": 0,
        "remoteImagesLoaded": False,
        "detailLoaded": False,
    }


def attachment_loader_for(token, quoted_message_id):
    def load_attachment(attachment_id):
        try:
            payload = api_request(
                "GET",
                f"/messages/{quoted_message_id}/attachments/{urllib.parse.quote(attachment_id)}",
                token,
            )
            return payload.get("data", "")
        except GmailBridgeError:
            return ""

    return load_attachment


def remote_image_loader_for(token=None):
    state = {"count": 0, "bytes": 0}

    def load_remote_image(url):
        if state["count"] >= REMOTE_IMAGE_LIMIT or state["bytes"] >= REMOTE_IMAGE_TOTAL_BYTES:
            return ""

        parsed = urllib.parse.urlparse(url)
        if parsed.scheme not in ("http", "https") or not parsed.netloc:
            return ""

        result = fetch_remote_image_to_cache(url, token)
        if not result["src"]:
            return ""
        if state["bytes"] + result["bytes"] > REMOTE_IMAGE_TOTAL_BYTES:
            return ""

        state["count"] += 1
        state["bytes"] += result["bytes"]
        return result["src"]

    return load_remote_image


def fetch_message(message_id, token=None, load_remote_images=False, force_html=False):
    active_token = token or ensure_token()
    quoted_id = urllib.parse.quote(message_id)
    started = time.perf_counter()
    payload = api_request("GET", f"/messages/{quoted_id}", active_token, query={"format": "full"})
    debug_timing("get", "fetch_full", started, messageId=message_id)
    normalize_started = time.perf_counter()
    message = normalize_message(
        payload,
        attachment_loader=attachment_loader_for(active_token, quoted_id),
        remote_image_loader=remote_image_loader_for(active_token) if load_remote_images else None,
        force_html=force_html,
    )
    debug_timing(
        "get",
        "normalize",
        normalize_started,
        messageId=message_id,
        remoteImages=message.get("remoteImageCount", 0),
        loadedImages=message.get("remoteImagesLoadedCount", 0),
    )
    return message


def fetch_message_summary(message_id, token=None):
    active_token = token or ensure_token()
    quoted_id = urllib.parse.quote(message_id)
    started = time.perf_counter()
    payload = api_request(
        "GET",
        f"/messages/{quoted_id}",
        active_token,
        query={"format": "metadata", "metadataHeaders": GMAIL_METADATA_HEADERS},
    )
    debug_timing("list", "fetch_metadata", started, messageId=message_id)
    return normalize_message_summary(payload)


def empty_cached_payload(folder, message_filter, query, page_token):
    return {
        "ok": True,
        "provider": "gmail",
        "folder": folder,
        "filter": message_filter,
        "query": query,
        "pageToken": page_token or "",
        "messages": [],
        "resultSizeEstimate": 0,
        "nextPageToken": "",
        "cached": False,
        "cacheMiss": True,
    }


def payload_matches_cache_request(payload, folder, message_filter, query, page_token):
    if payload.get("provider") != "gmail" or not isinstance(payload.get("messages"), list):
        return False
    if payload.get("folder") != folder or payload.get("filter") != message_filter:
        return False
    if (payload.get("query") or "") != (query or ""):
        return False
    if (payload.get("pageToken") or "") != (page_token or ""):
        return False
    try:
        cache_version = int(payload.get("cacheVersion", 0) or 0)
    except (TypeError, ValueError):
        cache_version = 0
    return cache_version == CACHE_VERSION


def cached_messages_by_id(token, folder, message_filter, query, page_token):
    cache_root = account_cache_root(token)
    if not cache_root.exists():
        return {}

    cached = {}
    for path in sorted(cache_root.glob("*.json"), key=lambda candidate: candidate.stat().st_mtime, reverse=True):
        try:
            payload = load_json(path)
        except (FileNotFoundError, json.JSONDecodeError, OSError):
            continue
        if not payload_matches_cache_request(payload, folder, message_filter, query, page_token):
            continue
        if upgrade_cached_payload(payload):
            payload["cachedAt"] = time.time()
            try:
                write_json(path, payload)
            except OSError:
                pass
        for message in payload.get("messages", []):
            message_id = message.get("messageId", "")
            if message_id and message_id not in cached:
                cached[message_id] = message_summary_for_list(message)
    return cached


def list_messages(folder, message_filter, query, limit, page_token="", force_refresh=False, use_cache=True, cache_only=False):
    total_started = time.perf_counter()
    token = ensure_token()
    search = build_query(folder, message_filter, query)
    requested_limit = max(1, min(int(limit), GMAIL_TOTAL_LIMIT))
    if use_cache and not force_refresh:
        cached_payload = read_cache(token, folder, message_filter, query, requested_limit, page_token)
        if cached_payload is not None:
            debug_timing("list", "cache_hit", total_started, count=len(cached_payload.get("messages", [])))
            return cached_payload
        if cache_only:
            debug_timing("list", "cache_miss", total_started, count=0)
            return empty_cached_payload(folder, message_filter, query, page_token)

    messages = []
    cached_messages = cached_messages_by_id(token, folder, message_filter, query, page_token) if use_cache else {}
    summary_index = read_summary_index(token) if use_cache else {}
    result_size_estimate = 0
    next_page_token = page_token or ""

    while len(messages) < requested_limit:
        page_size = min(GMAIL_PAGE_LIMIT, requested_limit - len(messages))
        request_query = {
            "maxResults": str(page_size),
            "q": search,
            "includeSpamTrash": "true" if folder == "Trash" else "false",
        }
        if next_page_token:
            request_query["pageToken"] = next_page_token

        page_started = time.perf_counter()
        response = api_request("GET", "/messages", token, query=request_query)
        debug_timing("list", "fetch_ids", page_started, pageSize=page_size)
        result_size_estimate = response.get("resultSizeEstimate", result_size_estimate)
        page_items = response.get("messages", [])
        next_page_token = response.get("nextPageToken", "")

        if not page_items:
            break

        for item in page_items:
            message_id = item.get("id", "")
            if not message_id:
                continue

            cached_message = cached_messages.get(message_id) or summary_index.get(message_id)
            if cached_message is not None:
                messages.append(message_summary_for_list(cached_message))
            else:
                summary = fetch_message_summary(message_id, token=token)
                remember_message_summary(token, summary)
                messages.append(summary)
            if len(messages) >= requested_limit:
                break

        if not next_page_token:
            break

    payload = {
        "ok": True,
        "provider": "gmail",
        "folder": folder,
        "filter": message_filter,
        "query": query,
        "pageToken": page_token or "",
        "messages": messages,
        "resultSizeEstimate": result_size_estimate or len(messages),
        "nextPageToken": next_page_token,
        "cached": False,
        "cacheMiss": False,
    }
    if use_cache:
        remember_message_summaries(token, messages)
        write_cache(token, folder, message_filter, query, requested_limit, page_token, payload)
    debug_timing("list", "total", total_started, count=len(messages), nextPage=bool(next_page_token))
    return payload


def extract_auth_code(*texts):
    joined = clean_display_text(" ".join(str(text or "") for text in texts))
    if not AUTH_CODE_CONTEXT_RE.search(joined):
        return ""

    for context in AUTH_CODE_CONTEXT_RE.finditer(joined):
        start = max(0, context.start() - 80)
        end = min(len(joined), context.end() + 120)
        window = joined[start:end]
        if re.search(r"\bpix\b", window, re.IGNORECASE) and not AUTH_STRONG_CONTEXT_RE.search(window):
            continue

        for candidate in AUTH_CODE_CANDIDATE_RE.finditer(window):
            code = re.sub(r"[^A-Za-z0-9]", "", candidate.group(1)).upper()
            if not (4 <= len(code) <= 8):
                continue
            if not any(ch.isdigit() for ch in code):
                continue
            if code in {"2FA", "OTP"}:
                continue
            return code

    return ""


def has_auth_code_context(*texts):
    joined = clean_display_text(" ".join(str(text or "") for text in texts))
    return bool(AUTH_CODE_CONTEXT_RE.search(joined))


def read_notification_state(path=NOTIFICATION_STATE_PATH):
    try:
        payload = load_json(pathlib.Path(path))
    except (FileNotFoundError, json.JSONDecodeError, OSError):
        payload = {}

    seen = payload.get("seenMessageIds", [])
    if not isinstance(seen, list):
        seen = []
    return {"seenMessageIds": [str(item) for item in seen if item]}


def write_notification_state(path, message_ids):
    deduped = []
    seen = set()
    for message_id in message_ids:
        if not message_id or message_id in seen:
            continue
        deduped.append(message_id)
        seen.add(message_id)
        if len(deduped) >= NOTIFICATION_SEEN_LIMIT:
            break

    write_json(pathlib.Path(path), {"seenMessageIds": deduped, "updatedAt": time.time()})


def copy_text_to_clipboard(text):
    text = str(text or "")
    if not text:
        return False
    for command in (["wl-copy"], ["xclip", "-selection", "clipboard"]):
        if not shutil.which(command[0]):
            continue
        try:
            subprocess.run(
                command,
                input=text,
                text=True,
                stdout=subprocess.DEVNULL,
                stderr=subprocess.DEVNULL,
                timeout=2,
                check=True,
            )
            return True
        except (OSError, subprocess.SubprocessError):
            continue
    return False


def astrea_notify_cli_path():
    configured = os.environ.get("ASTREA_NOTIFY_CLI", "").strip()
    candidates = [pathlib.Path(configured).expanduser()] if configured else []
    candidates.extend([
        ASTREA_NOTIFY_CLI,
        pathlib.Path.home() / ".local/share/Astrea-Rolling/bin/astrea-notify",
    ])
    for candidate in candidates:
        if candidate and candidate.exists():
            return candidate
    return None


def send_astrea_notification(
    title,
    body,
    urgency="normal",
    event_id="",
    thread_id="",
    collapse_key="",
    presentation="banner",
    interruption_level="active",
):
    cli_path = astrea_notify_cli_path()
    if not cli_path:
        return False

    command = [
        str(cli_path),
        "--app", "Astrea Mail",
        "--summary", str(title or "Astrea Mail"),
        "--body", str(body or ""),
        "--urgency", str(urgency or "normal"),
        "--presentation", str(presentation or "banner"),
        "--interruption-level", str(interruption_level or "active"),
    ]
    if event_id:
        command.extend(["--event-id", str(event_id)])
    if thread_id:
        command.extend(["--thread-id", str(thread_id)])
    if collapse_key:
        command.extend(["--collapse-key", str(collapse_key)])

    try:
        completed = subprocess.run(
            command,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=2,
            check=False,
        )
        return completed.returncode == 0
    except (OSError, subprocess.SubprocessError):
        return False


def send_desktop_notification(
    title,
    body,
    urgency="normal",
    event_id="",
    thread_id="",
    collapse_key="",
    presentation="banner",
    interruption_level="active",
):
    if send_astrea_notification(
        title,
        body,
        urgency=urgency,
        event_id=event_id,
        thread_id=thread_id,
        collapse_key=collapse_key,
        presentation=presentation,
        interruption_level=interruption_level,
    ):
        return True

    if not shutil.which("notify-send"):
        return False
    try:
        subprocess.run(
            ["notify-send", "-a", "Astrea Mail", "-u", urgency, str(title or "Astrea Mail"), str(body or "")],
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
            timeout=2,
            check=False,
        )
        return True
    except (OSError, subprocess.SubprocessError):
        return False


def write_island_email_event(event, path=None):
    target = pathlib.Path(path) if path is not None else ISLAND_EMAIL_EVENT_PATH
    now = time.time()
    payload = {
        "eventId": event.get("eventId") or f"email-{event.get('messageId', '')}-{int(now * 1000)}",
        "kind": "emailCode",
        "messageId": event.get("messageId", ""),
        "threadId": event.get("threadId", ""),
        "fromName": event.get("fromName", ""),
        "fromAddress": event.get("fromAddress", ""),
        "subject": event.get("subject", ""),
        "code": event.get("code", ""),
        "copied": bool(event.get("copied", False)),
        "createdAt": now,
        "expiresAt": now + NOTIFICATION_EVENT_TTL,
    }
    write_json(target, payload)


def poll_new_mail(limit=10, state_path=None, desktop_notify=True, copy_codes=True, island_notify=True):
    started = time.perf_counter()
    token = ensure_token()
    state_target = pathlib.Path(state_path) if state_path is not None else NOTIFICATION_STATE_PATH
    requested_limit = max(1, min(int(limit), 50))
    state = read_notification_state(state_target)

    response = api_request(
        "GET",
        "/messages",
        token,
        query={
            "maxResults": str(requested_limit),
            "q": "in:inbox",
            "includeSpamTrash": "false",
        },
    )
    inbox_ids = [item.get("id", "") for item in response.get("messages", []) if item.get("id", "")]
    if not state["seenMessageIds"]:
        write_notification_state(state_target, inbox_ids)
        debug_timing("notify", "bootstrap", started, count=len(inbox_ids))
        return {
            "ok": True,
            "provider": "gmail",
            "action": "notify",
            "bootstrapped": True,
            "newCount": 0,
            "events": [],
        }

    seen = set(state["seenMessageIds"])
    new_ids = [message_id for message_id in inbox_ids if message_id not in seen]
    events = []
    for message_id in reversed(new_ids):
        summary = fetch_message_summary(message_id, token=token)
        source_text = " ".join([summary.get("subject", ""), summary.get("preview", "")])
        code = extract_auth_code(source_text)
        detail = None
        if not code and has_auth_code_context(source_text):
            detail = fetch_message(message_id, token=token, load_remote_images=False, force_html=False)
            code = extract_auth_code(
                detail.get("subject", ""),
                detail.get("preview", ""),
                detail.get("body", ""),
            )
        event_source = detail or summary
        event = {
            "messageId": event_source.get("messageId", message_id),
            "threadId": event_source.get("threadId", ""),
            "folder": "Inbox",
            "fromName": event_source.get("fromName", ""),
            "fromAddress": event_source.get("fromAddress", ""),
            "subject": event_source.get("subject", ""),
            "preview": event_source.get("preview", ""),
            "timestamp": event_source.get("timestamp", ""),
            "tag": event_source.get("tag", "Gmail"),
            "starred": bool(event_source.get("starred", False)),
            "isRead": bool(event_source.get("isRead", False)),
            "importance": event_source.get("importance", "normal"),
            "hasAttachments": bool(event_source.get("hasAttachments", False)),
            "code": code,
            "hasCode": bool(code),
            "copied": False,
        }
        events.append(event)

    write_notification_state(state_target, inbox_ids + state["seenMessageIds"])

    for event in events:
        code = event["code"]
        if code and copy_codes:
            try:
                event["copied"] = copy_text_to_clipboard(code)
            except Exception:
                event["copied"] = False

        if desktop_notify:
            sender = event["fromName"] or event["fromAddress"] or "Gmail"
            event_id = f"email:{event['messageId']}"
            thread_id = f"email:{event['threadId'] or event['messageId']}"
            interruption_level = "time-sensitive" if code else "active"
            if code:
                title = "Email code copied"
                body = f"{code} from {sender}"
            else:
                title = "New email"
                subject = event["subject"] or "New email"
                body = f"{sender}: {subject}"
            try:
                send_desktop_notification(
                    title,
                    body,
                    urgency="normal",
                    event_id=event_id,
                    thread_id=thread_id,
                    collapse_key="email:inbox",
                    presentation="banner",
                    interruption_level=interruption_level,
                )
            except Exception:
                pass
        if code and island_notify:
            try:
                write_island_email_event(event)
            except OSError:
                pass

    debug_timing("notify", "total", started, count=len(events), codeCount=sum(1 for event in events if event["hasCode"]))
    return {
        "ok": True,
        "provider": "gmail",
        "action": "notify",
        "bootstrapped": False,
        "newCount": len(events),
        "events": events,
    }


def modify_plan(action):
    plans = {
        "read": {"endpoint": "modify", "body": {"removeLabelIds": ["UNREAD"]}},
        "unread": {"endpoint": "modify", "body": {"addLabelIds": ["UNREAD"]}},
        "star": {"endpoint": "modify", "body": {"addLabelIds": ["STARRED"]}},
        "unstar": {"endpoint": "modify", "body": {"removeLabelIds": ["STARRED"]}},
        "archive": {"endpoint": "modify", "body": {"removeLabelIds": ["INBOX"]}},
        "trash": {"endpoint": "trash", "body": {}},
        "inbox": {"endpoint": "untrash", "body": {}},
    }
    if action not in plans:
        raise GmailBridgeError(f"Unsupported Gmail action: {action}")
    return plans[action]


def message_patch_for_action(message_id, action):
    patches = {
        "read": {"isRead": True},
        "unread": {"isRead": False},
        "star": {"starred": True},
        "unstar": {"starred": False},
        "archive": {"folder": "Archive", "tag": "Archive"},
        "trash": {"folder": "Trash", "tag": "Trash"},
        "inbox": {"folder": "Inbox", "tag": "Gmail"},
    }
    patch = dict(patches.get(action, {}))
    patch["messageId"] = message_id
    return patch


def modify_message(message_id, action):
    token = ensure_token()
    quoted_id = urllib.parse.quote(message_id)
    plan = modify_plan(action)
    started = time.perf_counter()

    if plan["endpoint"] == "modify":
        api_request("POST", f"/messages/{quoted_id}/modify", token, body=plan["body"])
    else:
        api_request("POST", f"/messages/{quoted_id}/{plan['endpoint']}", token, body=plan["body"])
        if action == "inbox":
            api_request(
                "POST",
                f"/messages/{quoted_id}/modify",
                token,
                body={"addLabelIds": ["INBOX"], "removeLabelIds": ["TRASH"]},
            )

    message = message_patch_for_action(message_id, action)
    update_cached_message_patch(token, message_id, message)
    debug_timing("modify", action, started, messageId=message_id)
    return {
        "ok": True,
        "provider": "gmail",
        "action": action,
        "messageId": message_id,
        "message": message,
    }


def get_message(message_id, load_remote_images=False, force_html=False):
    started = time.perf_counter()
    token = None
    try:
        token = ensure_token()
    except GmailBridgeError:
        token = None
    if token is not None:
        cached_message = read_message_detail_cache(
            token,
            message_id,
            load_remote_images=load_remote_images,
            force_html=force_html,
        )
        if cached_message is not None:
            debug_timing("get", "detail_cache_hit", started, messageId=message_id)
            return {
                "ok": True,
                "provider": "gmail",
                "messageId": message_id,
                "message": cached_message,
                "cached": True,
            }

    if token is not None:
        message = fetch_message(
            message_id,
            token=token,
            load_remote_images=load_remote_images,
            force_html=force_html,
        )
    else:
        message = fetch_message(
            message_id,
            load_remote_images=load_remote_images,
            force_html=force_html,
        )
    if token is not None:
        write_message_detail_cache(token, message)
        remember_message_summary(token, message)
        update_cached_message_patch(token, message_id, message_summary_for_list(message))
    debug_timing(
        "get",
        "total",
        started,
        messageId=message_id,
        forceHtml=force_html,
        loadImages=load_remote_images,
    )
    return {"ok": True, "provider": "gmail", "messageId": message_id, "message": message, "cached": False}


def preview_message(message_id, load_remote_images=False):
    started = time.perf_counter()
    token = ensure_token()
    message = fetch_message(
        message_id,
        token=token,
        load_remote_images=load_remote_images,
        force_html=True,
    )
    write_message_detail_cache(token, message)
    remember_message_summary(token, message)

    snapshot_started = time.perf_counter()
    preview = render_message_preview_image(message)
    debug_timing(
        "preview",
        "snapshot",
        snapshot_started,
        messageId=message_id,
        loadImages=load_remote_images,
    )

    patch = {
        "messageId": message.get("messageId") or message_id,
        "htmlRenderMode": "html",
        "detailLoaded": True,
        "remoteImageCount": message.get("remoteImageCount", 0),
        "remoteImagesLoadedCount": message.get("remoteImagesLoadedCount", 0),
        "remoteImagesLoaded": bool(message.get("remoteImagesLoaded", False)),
    }
    for key in ("htmlLength", "htmlTableCount", "htmlSuppressed"):
        if key in message:
            patch[key] = message[key]
    if "links" in message:
        patch["links"] = message.get("links", [])
        patch["linkCount"] = len(patch["links"])
    patch.update(preview)
    update_cached_message_patch(token, message_id, patch)
    debug_timing("preview", "total", started, messageId=message_id, loadImages=load_remote_images)
    return {
        "ok": True,
        "provider": "gmail",
        "action": "preview",
        "messageId": message_id,
        "message": patch,
    }


def message_links(message_id):
    started = time.perf_counter()
    token = ensure_token()
    message = fetch_message(message_id, token=token, load_remote_images=False, force_html=False)
    write_message_detail_cache(token, message)
    remember_message_summary(token, message)
    links = message.get("links", [])
    patch = {
        "messageId": message.get("messageId") or message_id,
        "links": links,
        "linkCount": len(links),
    }
    update_cached_message_patch(token, message_id, patch)
    debug_timing("links", "total", started, messageId=message_id, count=len(links))
    return {
        "ok": True,
        "provider": "gmail",
        "action": "links",
        "messageId": message_id,
        "links": links,
        "linkCount": len(links),
    }


def message_preview_document(message):
    body = clean_display_text(message.get("htmlBody") or reader_text_to_html(message.get("body") or message.get("preview") or ""))
    return f"""<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <style>
    html, body {{
      margin: 0;
      padding: 0;
      background: #ffffff;
      color: #111827;
      font-family: -apple-system, BlinkMacSystemFont, "Inter", "Segoe UI", Arial, sans-serif;
    }}
    body {{
      width: {WEB_PREVIEW_WIDTH}px;
      overflow-x: hidden;
    }}
    img {{
      max-width: 100%;
      height: auto;
    }}
    table {{
      max-width: 100%;
    }}
    a {{
      color: #2563eb;
    }}
  </style>
</head>
<body>{body}</body>
</html>
"""


def message_preview_digest(message):
    source = json.dumps(
        {
            "messageId": message.get("messageId") or "",
            "htmlBody": message.get("htmlBody") or "",
            "remoteImagesLoadedCount": message.get("remoteImagesLoadedCount") or 0,
            "width": WEB_PREVIEW_WIDTH,
            "renderVersion": WEB_PREVIEW_RENDER_VERSION,
        },
        ensure_ascii=False,
        sort_keys=True,
    )
    return hashlib.sha256(source.encode("utf-8")).hexdigest()[:24]


def write_message_preview_html(message, digest):
    preview_dir = VIEWER_DIR / "snapshots"
    preview_dir.mkdir(parents=True, exist_ok=True)
    path = preview_dir / f"{digest}.html"
    temp_path = path.with_name(path.name + ".tmp")
    temp_path.write_text(message_preview_document(message), encoding="utf-8")
    os.replace(temp_path, path)
    try:
        os.chmod(path, 0o600)
    except OSError:
        pass
    return path


def png_dimensions(path):
    try:
        with open(path, "rb") as handle:
            header = handle.read(24)
    except OSError:
        return 0, 0
    if len(header) < 24 or header[:8] != b"\x89PNG\r\n\x1a\n" or header[12:16] != b"IHDR":
        return 0, 0
    return int.from_bytes(header[16:20], "big"), int.from_bytes(header[20:24], "big")


def normalized_preview_link_rects(value):
    if not isinstance(value, list):
        return []
    links = []
    for item in value[:LINK_EXTRACT_LIMIT * 3]:
        if not isinstance(item, dict):
            continue
        url = normalize_external_link(item.get("url", ""))
        if not url:
            continue
        try:
            x = max(0, float(item.get("x", 0) or 0))
            y = max(0, float(item.get("y", 0) or 0))
            width = max(0, float(item.get("width", 0) or 0))
            height = max(0, float(item.get("height", 0) or 0))
        except (TypeError, ValueError):
            continue
        if width < 3 or height < 3:
            continue
        links.append({
            "url": url,
            "label": clean_text(item.get("label", "") or url, LINK_LABEL_LIMIT),
            "x": int(round(x)),
            "y": int(round(y)),
            "width": int(round(width)),
            "height": int(round(height)),
        })
    return links


def read_preview_link_rects(path, stdout_text=""):
    try:
        if path.exists() and path.stat().st_size > 0:
            return normalized_preview_link_rects(load_json(path))
    except (json.JSONDecodeError, OSError):
        pass

    for line in (stdout_text or "").splitlines():
        marker_index = line.find(WEB_PREVIEW_LINKS_MARKER)
        if marker_index < 0:
            continue
        try:
            return normalized_preview_link_rects(json.loads(line[marker_index + len(WEB_PREVIEW_LINKS_MARKER):]))
        except json.JSONDecodeError:
            return []
    return []


def web_snapshot_command():
    for candidate in ("electron39", "electron"):
        executable = shutil.which(candidate)
        if executable and WEB_ELECTRON_SNAPSHOT_JS.exists():
            return [executable, "--no-sandbox", str(WEB_ELECTRON_SNAPSHOT_JS)]

    viewer = shutil.which("qml6") or shutil.which("qml")
    if viewer and WEB_SNAPSHOT_QML.exists():
        return [viewer, str(WEB_SNAPSHOT_QML)]

    raise GmailBridgeError("No Web preview snapshot renderer found")


def render_message_preview_image(message, width=WEB_PREVIEW_WIDTH, max_height=WEB_PREVIEW_MAX_HEIGHT):
    if not clean_display_text(message.get("htmlBody") or "").strip():
        raise GmailBridgeError("No HTML body to render")

    digest = message_preview_digest(message)
    snapshot_dir = VIEWER_DIR / "snapshots"
    snapshot_dir.mkdir(parents=True, exist_ok=True)
    image_path = snapshot_dir / f"{digest}.png"
    links_path = snapshot_dir / f"{digest}.links.json"

    if image_path.exists() and image_path.stat().st_size > 0:
        image_width, image_height = png_dimensions(image_path)
        if image_width > 0 and image_height > 0:
            return {
                "webPreviewUrl": image_path.as_uri(),
                "webPreviewWidth": image_width,
                "webPreviewHeight": image_height,
                "webPreviewLinks": read_preview_link_rects(links_path),
                "webPreviewLinksReady": True,
            }

    html_path = write_message_preview_html(message, digest)
    command = web_snapshot_command() + [
        "--html",
        str(html_path),
        "--output",
        str(image_path),
        "--links-output",
        str(links_path),
        "--width",
        str(int(width)),
        "--max-height",
        str(int(max_height)),
    ]

    try:
        completed = subprocess.run(
            command,
            stdout=subprocess.PIPE,
            stderr=subprocess.PIPE,
            text=True,
            timeout=WEB_PREVIEW_TIMEOUT,
            check=True,
        )
    except subprocess.TimeoutExpired as exc:
        raise GmailBridgeError("Web preview snapshot timed out") from exc
    except subprocess.CalledProcessError as exc:
        stderr = (exc.stderr or exc.stdout or "").strip()
        message_text = stderr.splitlines()[-1] if stderr else str(exc)
        raise GmailBridgeError(f"Web preview snapshot failed: {message_text}") from exc

    if not image_path.exists() or image_path.stat().st_size == 0:
        stderr = (getattr(completed, "stderr", "") or "").strip()
        suffix = f": {stderr.splitlines()[-1]}" if stderr else ""
        raise GmailBridgeError(f"Web preview snapshot was not created{suffix}")

    image_width, image_height = png_dimensions(image_path)
    if image_width <= 0 or image_height <= 0:
        raise GmailBridgeError("Web preview snapshot is not a valid PNG")

    return {
        "webPreviewUrl": image_path.as_uri(),
        "webPreviewWidth": image_width,
        "webPreviewHeight": image_height,
        "webPreviewLinks": read_preview_link_rects(links_path, getattr(completed, "stdout", "")),
        "webPreviewLinksReady": True,
    }


def message_viewer_document(message):
    subject = html.escape(clean_display_text(message.get("subject") or "Email"), quote=False)
    sender = html.escape(clean_display_text(message.get("fromName") or message.get("fromAddress") or ""), quote=False)
    timestamp = html.escape(clean_display_text(message.get("timestamp") or ""), quote=False)
    body = clean_display_text(message.get("htmlBody") or reader_text_to_html(message.get("body") or message.get("preview") or ""))
    return f"""<!doctype html>
<html>
<head>
  <meta charset="utf-8">
  <meta name="viewport" content="width=device-width, initial-scale=1">
  <title>{subject}</title>
  <style>
    :root {{
      color-scheme: light dark;
      background: #111827;
      color: #111827;
      font-family: -apple-system, BlinkMacSystemFont, "Inter", "Segoe UI", sans-serif;
    }}
    body {{
      margin: 0;
      background: #111827;
    }}
    .frame {{
      max-width: 980px;
      margin: 0 auto;
      padding: 28px;
    }}
    .meta {{
      box-sizing: border-box;
      max-width: 760px;
      margin: 0 auto 16px auto;
      color: #d1d5db;
      font-size: 13px;
      line-height: 1.45;
    }}
    .subject {{
      color: #f9fafb;
      font-size: 20px;
      font-weight: 650;
      margin-bottom: 4px;
    }}
    .mail {{
      box-sizing: border-box;
      max-width: 760px;
      margin: 0 auto;
      background: #ffffff;
      color: #111827;
      border-radius: 10px;
      overflow: hidden;
      box-shadow: 0 22px 80px rgba(0, 0, 0, 0.32);
    }}
    .mail img {{
      max-width: 100%;
      height: auto;
    }}
    .mail table {{
      max-width: 100%;
    }}
    a {{
      color: #2563eb;
    }}
    @media (prefers-color-scheme: dark) {{
      .mail {{
        background: #ffffff;
        color: #111827;
      }}
    }}
  </style>
</head>
<body>
  <div class="frame">
    <header class="meta">
      <div class="subject">{subject}</div>
      <div>{sender}{(" &middot; " + timestamp) if timestamp else ""}</div>
    </header>
    <main class="mail">{body}</main>
  </div>
</body>
</html>
"""


def write_message_viewer_html(message):
    message_id = message.get("messageId") or "message"
    digest = hashlib.sha256(message_id.encode("utf-8")).hexdigest()[:24]
    VIEWER_DIR.mkdir(parents=True, exist_ok=True)
    path = VIEWER_DIR / f"{digest}.html"
    temp_path = path.with_name(path.name + ".tmp")
    temp_path.write_text(message_viewer_document(message), encoding="utf-8")
    os.replace(temp_path, path)
    try:
        os.chmod(path, 0o600)
    except OSError:
        pass
    return path


def render_message_for_viewer(message_id, load_remote_images=True):
    message = fetch_message(message_id, load_remote_images=load_remote_images, force_html=True)
    path = write_message_viewer_html(message)
    return {
        "ok": True,
        "provider": "gmail",
        "messageId": message_id,
        "htmlPath": str(path),
        "remoteImageCount": message.get("remoteImageCount", 0),
        "remoteImagesLoadedCount": message.get("remoteImagesLoadedCount", 0),
    }


def is_running_viewer(pid):
    try:
        cmdline = pathlib.Path(f"/proc/{pid}/cmdline").read_text(encoding="utf-8", errors="replace")
    except OSError:
        return False
    return str(WEB_VIEWER_QML) in cmdline


def stop_existing_viewer():
    try:
        pid = int(WEB_VIEWER_PID.read_text(encoding="utf-8").strip())
    except (OSError, ValueError):
        return
    if pid <= 0 or not is_running_viewer(pid):
        return
    try:
        os.kill(pid, signal.SIGTERM)
    except OSError:
        pass


def viewer_geometry_args(geometry=None):
    if not geometry:
        return []
    args = []
    for key in ("x", "y", "width", "height"):
        value = geometry.get(key)
        if value is not None:
            args.extend([f"--{key}", str(int(value))])
    return args


def open_message_viewer(message_id, load_remote_images=True, geometry=None, replace_existing=True):
    payload = render_message_for_viewer(message_id, load_remote_images=load_remote_images)
    viewer = shutil.which("qml6") or shutil.which("qml")
    if not viewer:
        raise GmailBridgeError("Qt WebEngine viewer is not available: qml6 not found")
    if not WEB_VIEWER_QML.exists():
        raise GmailBridgeError(f"Missing WebEngine viewer: {WEB_VIEWER_QML}")

    if replace_existing:
        stop_existing_viewer()

    try:
        process = subprocess.Popen(
            [viewer, str(WEB_VIEWER_QML)] + viewer_geometry_args(geometry) + [payload["htmlPath"]],
            start_new_session=True,
            stdout=subprocess.DEVNULL,
            stderr=subprocess.DEVNULL,
        )
    except OSError as exc:
        raise GmailBridgeError(str(exc)) from exc

    VIEWER_DIR.mkdir(parents=True, exist_ok=True)
    WEB_VIEWER_PID.write_text(str(process.pid), encoding="utf-8")
    payload["message"] = "Web preview updated" if replace_existing else "Opened original message"
    return payload


def create_send_payload(to, subject, body):
    message = EmailMessage()
    message["To"] = to
    message["Subject"] = subject
    message.set_content(body or "")
    raw = base64.urlsafe_b64encode(message.as_bytes()).decode("ascii").rstrip("=")
    return {"raw": raw}


def send_message(to, subject, body):
    token = ensure_token()
    payload = api_request("POST", "/messages/send", token, body=create_send_payload(to, subject, body))
    return {"ok": True, "provider": "gmail", "messageId": payload.get("id", ""), "threadId": payload.get("threadId", "")}


def status_payload():
    path = credentials_path()
    token = read_token()
    state = token_state(token)
    configured = path.exists()
    authenticated = configured and state in ("valid", "refreshable")

    if not configured:
        message = f"Add Gmail OAuth client at {path}"
    elif authenticated:
        message = "Gmail ready"
    else:
        message = "Connect Gmail"

    return {
        "ok": True,
        "provider": "gmail",
        "configured": configured,
        "authenticated": authenticated,
        "credentialsPath": str(path),
        "tokenPath": str(TOKEN_PATH),
        "tokenState": state,
        "account": token.get("account", ""),
        "scopes": SCOPES,
        "messages": [],
        "message": message,
    }


def auth_payload():
    token = authenticate()
    payload = status_payload()
    payload["account"] = token.get("account", "")
    payload["message"] = "Gmail connected"
    return payload
