const fs = require("fs");
const path = require("path");
const { app, BrowserWindow } = require("electron");

function argString(name, fallback = "") {
  const index = process.argv.indexOf(name);
  if (index < 0 || index + 1 >= process.argv.length) {
    return fallback;
  }
  return process.argv[index + 1];
}

function argNumber(name, fallback) {
  const parsed = Number(argString(name, String(fallback)));
  return Number.isFinite(parsed) ? parsed : fallback;
}

function finish(code, message) {
  if (message) {
    const stream = code === 0 ? process.stdout : process.stderr;
    stream.write(`${message}\n`);
  }
  app.exit(code);
}

const htmlPath = argString("--html");
const outputPath = argString("--output");
const linksOutputPath = argString("--links-output");
const requestedWidth = Math.max(320, argNumber("--width", 820));
const maxHeight = Math.max(640, argNumber("--max-height", 16000));

app.commandLine.appendSwitch("disable-gpu");
app.commandLine.appendSwitch("disable-extensions");
app.commandLine.appendSwitch("disable-background-networking");
app.commandLine.appendSwitch("disable-sync");
app.commandLine.appendSwitch("metrics-recording-only");
app.commandLine.appendSwitch("no-first-run");
app.commandLine.appendSwitch("no-sandbox");
app.commandLine.appendSwitch("force-device-scale-factor", "1");

app.whenReady().then(async () => {
  if (!htmlPath || !outputPath) {
    finish(2, "Missing --html or --output");
    return;
  }

  const win = new BrowserWindow({
    show: false,
    width: requestedWidth,
    height: 640,
    backgroundColor: "#ffffff",
    webPreferences: {
      contextIsolation: true,
      nodeIntegration: false,
      sandbox: true,
      webSecurity: true,
      images: true,
      plugins: false,
      webgl: false,
      offscreen: true,
    },
  });

  win.webContents.setWindowOpenHandler(() => ({ action: "deny" }));
  win.webContents.on("will-navigate", event => event.preventDefault());
  win.webContents.session.setPermissionRequestHandler((_webContents, _permission, callback) => callback(false));

  try {
    await win.loadFile(path.resolve(htmlPath));
    await new Promise(resolve => setTimeout(resolve, 160));

    const measuredHeight = await win.webContents.executeJavaScript(
      "(() => { const body = document.body; const rect = body.getBoundingClientRect(); const childBottom = Array.from(body.children).reduce((bottom, child) => Math.max(bottom, child.getBoundingClientRect().bottom), 0); return Math.ceil(Math.max(rect.bottom, childBottom)); })()",
      true
    );
    const height = Math.max(80, Math.min(maxHeight, Number(measuredHeight) || 640));
    win.setContentSize(requestedWidth, height);
    await new Promise(resolve => setTimeout(resolve, 120));

    const linkRects = await win.webContents.executeJavaScript(`
      (() => {
        function normalizeExternalLink(raw) {
          let value = String(raw || "").trim();
          if (!value) return "";
          if (value.indexOf("//") === 0) value = "https:" + value;
          if (!/^(https?:|mailto:)/i.test(value)) return "";
          try {
            const parsed = new URL(value);
            if ((parsed.protocol === "http:" || parsed.protocol === "https:") && parsed.host) return parsed.href;
            if (parsed.protocol === "mailto:" && parsed.pathname) return parsed.href;
          } catch (_error) {
            return "";
          }
          return "";
        }

        function cleanLabel(value) {
          return String(value || "").replace(/\\s+/g, " ").trim().slice(0, 96);
        }

        const links = [];
        const seen = new Set();
        for (const anchor of Array.from(document.querySelectorAll("a[href]"))) {
          const url = normalizeExternalLink(anchor.getAttribute("href"));
          if (!url) continue;
          const label = cleanLabel(anchor.innerText || anchor.getAttribute("aria-label") || anchor.getAttribute("title") || url) || url;
          for (const rect of Array.from(anchor.getClientRects())) {
            const width = Math.round(rect.width);
            const height = Math.round(rect.height);
            if (width < 3 || height < 3) continue;
            const x = Math.max(0, Math.round(rect.left));
            const y = Math.max(0, Math.round(rect.top));
            const key = [url, x, y, width, height].join(":");
            if (seen.has(key)) continue;
            seen.add(key);
            links.push({ url, label, x, y, width, height });
            if (links.length >= 120) return links;
          }
        }
        return links;
      })()
    `, true);

    if (linksOutputPath) {
      fs.mkdirSync(path.dirname(linksOutputPath), { recursive: true });
      fs.writeFileSync(linksOutputPath, JSON.stringify(Array.isArray(linkRects) ? linkRects : []));
    }

    const image = await win.webContents.capturePage({
      x: 0,
      y: 0,
      width: requestedWidth,
      height,
    });
    fs.mkdirSync(path.dirname(outputPath), { recursive: true });
    fs.writeFileSync(outputPath, image.toPNG());
    finish(0);
  } catch (error) {
    finish(2, error && error.message ? error.message : String(error));
  }
});

app.on("window-all-closed", event => {
  event.preventDefault();
});
