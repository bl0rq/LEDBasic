#pragma once

#include <Arduino.h>

// Option lists match firmware/src/config.h. Factory defaults are preselected
// so the LED form is usable before the script fills live values.

static const char kPage[] PROGMEM = R"page(<!doctype html>
<html>
<head>
<meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>LEDBasic</title>
<script>
try {
  var saved = localStorage.getItem("ledbasic-theme");
  if (saved === "light" || saved === "dark") document.documentElement.setAttribute("data-theme", saved);
} catch (e) {}
</script>
<style>
:root {
  color-scheme: light;
  --bg: #f6f3ec;
  --fg: #1b1916;
  --muted: #5e584e;
  --line: #ddd6c8;
  --accent: #8f3d1b;
  --danger: #9f1239;
  --field: #fffcf7;
  --chip: #e7e1d6;
}
@media (prefers-color-scheme: dark) {
  :root:not([data-theme="light"]) {
    color-scheme: dark;
    --bg: #12110f;
    --fg: #ece8e1;
    --muted: #a69e93;
    --line: #3c362e;
    --accent: #e2a07a;
    --danger: #fb7185;
    --field: #1c1a17;
    --chip: #2a261f;
  }
}
html[data-theme="light"] {
  color-scheme: light;
  --bg: #f6f3ec; --fg: #1b1916; --muted: #5e584e; --line: #ddd6c8;
  --accent: #8f3d1b; --danger: #9f1239; --field: #fffcf7; --chip: #e7e1d6;
}
html[data-theme="dark"] {
  color-scheme: dark;
  --bg: #12110f; --fg: #ece8e1; --muted: #a69e93; --line: #3c362e;
  --accent: #e2a07a; --danger: #fb7185; --field: #1c1a17; --chip: #2a261f;
}
body { font: 16px/1.45 system-ui, sans-serif; background: var(--bg); color: var(--fg); margin: 0; }
main { max-width: 40rem; margin: 0 auto; padding: 1rem; }
header { display: flex; align-items: baseline; justify-content: space-between; gap: 1rem; }
h1 { font-size: 1.35rem; font-weight: 600; margin: 0; }
h2 { font-size: 1rem; font-weight: 600; margin: 1.5rem 0 .4rem; }
p { margin: .4rem 0; }
button, input, select { font: inherit; color: inherit; background: var(--field); border: 1px solid var(--line); padding: .35rem .55rem; }
button { cursor: pointer; }
button.primary { color: var(--accent); }
label { display: block; margin: .5rem 0; }
.muted { color: var(--muted); }
.error { color: var(--danger); }
.hidden { display: none; }
table { width: 100%; border-collapse: collapse; }
td, th { text-align: left; padding: .4rem .15rem; border-bottom: 1px solid var(--line); vertical-align: middle; }
.row { display: flex; flex-wrap: wrap; gap: .4rem; align-items: center; margin: .35rem 0; }
canvas { width: 100%; height: 28px; background: var(--chip); display: block; }
.param { margin: .8rem 0; }
.spread { display: flex; justify-content: space-between; gap: .5rem; }
input[type=range] { width: 100%; padding: 0; accent-color: var(--accent); }
:focus-visible { outline: 2px solid var(--accent); outline-offset: 2px; }
</style>
</head>
<body>
<main>
<header>
  <h1>LEDBasic</h1>
  <button type=button id=theme>Dark</button>
</header>
<p id=error class="error hidden"></p>
<p id=restart class=hidden>Restarting. Reconnect, then open this page again.</p>
<canvas id=strip width=640 height=28 aria-label="LED preview"></canvas>
<label class=spread>Brightness <span id=bright-read class=muted>255</span></label>
<input id=bright type=range min=0 max=255 step=1 value=255>
<label class=spread>Palette <select id=palette></select></label>
<h2>Status</h2>
<p id=link class=muted>Connecting…</p>
<p id=runline></p>
<p id=fault class="error hidden"></p>
<h2>Wi-Fi</h2>
<p class=muted>Access point password is ledbasic. Hold the button on GPIO17 for 5 seconds to forget the network.</p>
<p><button type=button id=scan>Scan networks</button></p>
<div id=networks></div>
<form id=join>
  <label>SSID <input name=ssid required></label>
  <label>Password <input name=password type=password></label>
  <button type=submit class=primary>Join</button>
</form>
<form method=post action=/api/wifi/reset>
  <button type=submit>Forget network</button>
</form>
<h2>LEDs</h2>
<form method=post action=/api/led id=leds>
  <label>Type <select name=type>
    <option>ws2812</option>
    <option>ws2811</option>
  </select></label>
  <label>Order <select name=order>
    <option>RGB</option><option>RBG</option><option selected>GRB</option>
    <option>GBR</option><option>BRG</option><option>BGR</option>
  </select></label>
  <label>Length <input name=length type=number min=1 max=1024 value=60>
    <button type=button id=find>Find</button></label>
  <div id=finder class=hidden>
    <p class=muted>The start of the strip is white, red, green, blue. The end marker is blue, green, white. Move it until that white pixel is the last LED.</p>
    <p id=finder-read></p>
    <div class=row>
      <button type=button id=finder-back10>-10</button>
      <button type=button id=finder-back>Back</button>
      <button type=button id=finder-forward>Forward</button>
      <button type=button id=finder-fwd10>+10</button>
    </div>
    <div class=row>
      <button type=button id=finder-use class=primary>Use this length</button>
      <button type=button id=finder-cancel>Cancel</button>
    </div>
  </div>
  <label>Pin <select name=pin>
    <option selected>16</option><option>14</option><option>13</option>
    <option>12</option><option>4</option><option>2</option>
  </select></label>
  <button type=submit class=primary>Save</button>
</form>
<h2>Programs</h2>
<table>
  <thead><tr><th>Name</th><th></th><th></th></tr></thead>
  <tbody id=programs></tbody>
</table>
<h2>Params</h2>
<p id=params-note class=muted>Run a program to adjust its parameters.</p>
<div id=params></div>
<h2>Upload</h2>
<form id=upload method=post action=/api/program/upload enctype=multipart/form-data>
  <label>Name <input name=name required></label>
  <label>File <input name=file type=file accept=.bas,.txt required></label>
  <label><input name=activate type=checkbox value=1> Run after upload</label>
  <button type=submit class=primary>Upload</button>
</form>
<noscript><p>Status, programs, and the strip need JavaScript.</p></noscript>
</main>
<script>
var strip = document.getElementById("strip");
var lastColors = [];
var bright = document.getElementById("bright");
var brightRead = document.getElementById("bright-read");
var brightHold = false;
var brightDown = false;
var brightBusy = false;
var brightWant = null;
var brightSent = null;

function brightSettled() {
  if (!brightDown && !brightBusy && brightWant === brightSent) brightHold = false;
}
function flushBright() {
  if (brightBusy || brightWant == null || brightWant === brightSent) {
    brightSettled();
    return;
  }
  var value = brightWant;
  brightBusy = true;
  fetch("/api/brightness", {
    method: "POST",
    headers: { "Content-Type": "application/json", "Accept": "application/json" },
    body: JSON.stringify({ brightness: value })
  }).then(function (response) { return response.json(); }).then(function (data) {
    brightSent = value;
    brightBusy = false;
    if (!data.ok) showError(data.error || "could not set brightness");
    flushBright();
  }).catch(function () {
    brightBusy = false;
    showError("could not set brightness");
    brightSettled();
  });
}
function queueBright() {
  brightHold = true;
  brightRead.textContent = bright.value;
  brightWant = Number(bright.value);
  flushBright();
}
bright.addEventListener("pointerdown", function () { brightDown = true; brightHold = true; });
bright.addEventListener("pointerup", function () { brightDown = false; brightSettled(); });
bright.addEventListener("pointercancel", function () { brightDown = false; brightSettled(); });
bright.addEventListener("input", queueBright);
bright.addEventListener("change", queueBright);

var palette = document.getElementById("palette");
var paletteReady = false;
function loadPalette() {
  return fetch("/api/palette").then(function (response) { return response.json(); }).then(function (data) {
    var current = data.palette || "";
    palette.textContent = "";
    (data.palettes || []).forEach(function (name) {
      var option = document.createElement("option");
      option.value = name;
      option.textContent = name;
      if (name === current) option.selected = true;
      palette.appendChild(option);
    });
    paletteReady = true;
  });
}
palette.addEventListener("change", function () {
  fetch("/api/palette", {
    method: "POST",
    headers: { "Content-Type": "application/json", "Accept": "application/json" },
    body: JSON.stringify({ palette: palette.value })
  }).then(function (response) { return response.json(); }).then(function (data) {
    if (!data.ok) showError(data.error || "could not set palette");
    else showError("");
  }).catch(function () { showError("could not set palette"); });
});

function themeNow() {
  var set = document.documentElement.getAttribute("data-theme");
  if (set === "light" || set === "dark") return set;
  return window.matchMedia("(prefers-color-scheme: dark)").matches ? "dark" : "light";
}
function paintThemeButton() {
  document.getElementById("theme").textContent = themeNow() === "dark" ? "Light" : "Dark";
}
paintThemeButton();
document.getElementById("theme").addEventListener("click", function () {
  var next = themeNow() === "dark" ? "light" : "dark";
  document.documentElement.setAttribute("data-theme", next);
  try { localStorage.setItem("ledbasic-theme", next); } catch (e) {}
  paintThemeButton();
});

function showError(text) {
  var node = document.getElementById("error");
  node.textContent = text || "";
  node.classList.toggle("hidden", !text);
}
function showRestart() {
  document.getElementById("restart").classList.remove("hidden");
  setTimeout(function () { location.reload(); }, 8000);
}

function draw(colors) {
  lastColors = colors || [];
  var width = strip.clientWidth || 320;
  var height = 28;
  var ratio = window.devicePixelRatio || 1;
  strip.width = Math.round(width * ratio);
  strip.height = Math.round(height * ratio);
  var g = strip.getContext("2d");
  g.setTransform(ratio, 0, 0, ratio, 0, 0);
  g.clearRect(0, 0, width, height);
  var n = lastColors.length;
  if (!n) return;
  var gap = n > 1 ? 1 : 0;
  var cell = (width - gap * (n - 1)) / n;
  for (var i = 0; i < n; i++) {
    var px = lastColors[i];
    g.fillStyle = "rgb(" + px[0] + "," + px[1] + "," + px[2] + ")";
    g.fillRect(i * (cell + gap), 0, Math.ceil(cell), height);
  }
}
window.addEventListener("resize", function () { draw(lastColors); });

function paintStatus(data) {
  var link = document.getElementById("link");
  link.textContent = (data.mode || "") + "  " + (data.ssid || "") + "  " + (data.ip || "");
  var name = data.unsaved ? "unsaved buffer" : (data.program || "");
  document.getElementById("runline").textContent =
    name + "  ·  brightness " + data.brightness + "  ·  " + data.fps + " fps  ·  " + (data.version || "");
  var fault = document.getElementById("fault");
  var message = data.error && data.error.message ? data.error.message : "";
  fault.textContent = message;
  fault.classList.toggle("hidden", !message);
  if (!brightHold && data.brightness != null) {
    bright.value = data.brightness;
    brightRead.textContent = String(data.brightness);
  }
  if (paletteReady && data.palette && document.activeElement !== palette) {
    palette.value = data.palette;
  }
  var shown = (data.unsaved ? "unsaved:" : "saved:") + (data.program || "");
  if (shown !== paramProgram) {
    paramProgram = shown;
    loadParams();
  }
}

function postForm(url, fields) {
  var body = new URLSearchParams();
  Object.keys(fields).forEach(function (key) { body.set(key, fields[key]); });
  return fetch(url, {
    method: "POST",
    headers: { "Content-Type": "application/x-www-form-urlencoded", "Accept": "application/json" },
    body: body.toString()
  }).then(function (response) {
    return response.json().then(function (data) {
      data._ok = response.ok;
      return data;
    });
  });
}
function report(data) {
  if (data.restart) { showRestart(); return; }
  if (data.ok) {
    showError("");
    loadPrograms().catch(function () {});
    refresh();
    loadParams();
    return;
  }
  var message = data.error || "request failed";
  if (data.diagnostics && data.diagnostics.length && data.diagnostics[0].message) {
    message += " (" + data.diagnostics[0].message + ")";
  }
  showError(message);
}

function loadPrograms() {
  return fetch("/api/programs").then(function (r) { return r.json(); }).then(function (list) {
    var body = document.getElementById("programs");
    body.textContent = "";
    list.forEach(function (item) {
      var row = document.createElement("tr");
      row.setAttribute("data-name", item.name);
      if (item.active) row.classList.add("running");
      var name = document.createElement("td");
      name.textContent = item.name + (item.builtin ? " (built-in)" : "") + (item.active ? " running" : "");
      var run = document.createElement("td");
      var runButton = document.createElement("button");
      runButton.type = "button";
      runButton.textContent = "Run";
      runButton.addEventListener("click", function () {
        postForm("/api/program/activate", { name: item.name }).then(report).catch(function () {
          showError("could not run program");
        });
      });
      run.appendChild(runButton);
      var del = document.createElement("td");
      if (!item.builtin) {
        var delButton = document.createElement("button");
        delButton.type = "button";
        delButton.textContent = "Delete";
        delButton.addEventListener("click", function () {
          if (!window.confirm("Delete " + item.name + "?")) return;
          postForm("/api/program/delete", { name: item.name }).then(report).catch(function () {
            showError("could not delete program");
          });
        });
        del.appendChild(delButton);
      }
      row.appendChild(name);
      row.appendChild(run);
      row.appendChild(del);
      body.appendChild(row);
    });
  });
}

var paramProgram = "";

function formatNum(value) {
  var number = Number(value);
  if (!isFinite(number)) return String(value);
  if (Math.abs(number - Math.round(number)) < 0.0001) return String(Math.round(number));
  return String(Math.round(number * 100) / 100);
}

function setParam(name, value) {
  return fetch("/api/params", {
    method: "POST",
    headers: { "Content-Type": "application/json", "Accept": "application/json" },
    body: JSON.stringify({ name: name, value: value })
  }).then(function (response) { return response.json(); }).then(function (data) {
    if (!data.ok) showError(data.error || "could not set parameter");
    else showError("");
  }).catch(function () { showError("could not set parameter"); });
}

function paramControl(param) {
  var wrap = document.createElement("div");
  wrap.className = "param";
  var row = document.createElement("div");
  row.className = "spread";
  var label = document.createElement("span");
  label.textContent = param.name;
  var readout = document.createElement("span");
  readout.className = "muted";
  row.appendChild(label);
  row.appendChild(readout);
  wrap.appendChild(row);

  if (param.type === "boolean") {
    var check = document.createElement("input");
    check.type = "checkbox";
    check.checked = !!param.value;
    readout.textContent = check.checked ? "on" : "off";
    check.addEventListener("change", function () {
      readout.textContent = check.checked ? "on" : "off";
      setParam(param.name, check.checked);
    });
    wrap.appendChild(check);
    return wrap;
  }

  if (param.type === "enum") {
    var select = document.createElement("select");
    (param.values || []).forEach(function (name, index) {
      var option = document.createElement("option");
      option.value = String(index);
      option.textContent = name;
      if (index === param.value) option.selected = true;
      select.appendChild(option);
    });
    readout.textContent = (param.values && param.values[param.value]) || "";
    select.addEventListener("change", function () {
      readout.textContent = select.options[select.selectedIndex].textContent;
      setParam(param.name, Number(select.value));
    });
    wrap.appendChild(select);
    return wrap;
  }

  var range = document.createElement("input");
  range.type = "range";
  range.min = param.min;
  range.max = param.max;
  range.step = param.step || 1;
  range.value = param.value;
  readout.textContent = formatNum(param.value);
  var lane = { busy: false, want: null, sent: Number(param.value) };
  function flushRange() {
    if (lane.busy || lane.want == null || lane.want === lane.sent) return;
    var value = lane.want;
    lane.busy = true;
    setParam(param.name, value).then(function () {
      lane.sent = value;
      lane.busy = false;
      flushRange();
    });
  }
  function queueRange() {
    readout.textContent = formatNum(range.value);
    lane.want = Number(range.value);
    flushRange();
  }
  range.addEventListener("input", queueRange);
  range.addEventListener("change", queueRange);
  wrap.appendChild(range);
  return wrap;
}

function loadParams() {
  return fetch("/api/params").then(function (response) { return response.json(); }).then(function (list) {
    var box = document.getElementById("params");
    var note = document.getElementById("params-note");
    box.textContent = "";
    if (!list || !list.length) {
      note.textContent = paramProgram ? "This program has no parameters." : "Run a program to adjust its parameters.";
      note.classList.remove("hidden");
      return;
    }
    note.textContent = "";
    note.classList.add("hidden");
    list.forEach(function (param) { box.appendChild(paramControl(param)); });
  }).catch(function () {});
}

function showFinder(led) {
  var on = !!(led.measure && led.measure.active);
  document.getElementById("finder").classList.toggle("hidden", !on);
  document.getElementById("find").classList.toggle("hidden", on);
  if (on) {
    document.getElementById("finder-read").textContent = "Length " + led.measure.end + ". White is the last LED.";
  }
}
function loadLeds(tries) {
  if (tries == null) tries = 15;
  return fetch("/api/led").then(function (r) { return r.json(); }).then(function (led) {
    var form = document.getElementById("leds");
    ["type", "order", "length", "pin"].forEach(function (key) {
      var field = form.elements.namedItem(key);
      if (led[key] != null && field) field.value = String(led[key]);
    });
    showFinder(led);
  }).catch(function () {
    if (tries > 0) setTimeout(function () { loadLeds(tries - 1); }, 1000);
  });
}
function measure(action, by) {
  var body = { action: action };
  if (by != null) body.by = by;
  return fetch("/api/measure", {
    method: "POST",
    headers: { "Content-Type": "application/json", "Accept": "application/json" },
    body: JSON.stringify(body)
  }).then(function (response) { return response.json(); }).then(function (data) {
    if (data.restart) { showRestart(); return; }
    if (!data.ok) { showError(data.error || "could not measure"); return; }
    showError("");
    if (data.end != null) {
      document.getElementById("finder-read").textContent = "Length " + data.end + ". White is the last LED.";
      var lengthField = document.getElementById("leds").elements.namedItem("length");
      if (lengthField) lengthField.value = String(data.end);
    }
    if (data.length != null) {
      var savedLength = document.getElementById("leds").elements.namedItem("length");
      if (savedLength) savedLength.value = String(data.length);
    }
  }).catch(function () { showError("could not measure"); });
}
document.getElementById("find").addEventListener("click", function () { measure("start"); });
document.getElementById("finder-back").addEventListener("click", function () { measure("move", -1); });
document.getElementById("finder-forward").addEventListener("click", function () { measure("move", 1); });
document.getElementById("finder-back10").addEventListener("click", function () { measure("move", -10); });
document.getElementById("finder-fwd10").addEventListener("click", function () { measure("move", 10); });
document.getElementById("finder-use").addEventListener("click", function () { measure("save"); });
document.getElementById("finder-cancel").addEventListener("click", function () { measure("cancel"); });

function joinNetwork(ssid, password) {
  return fetch("/api/wifi", {
    method: "POST",
    headers: { "Content-Type": "application/json", "Accept": "application/json" },
    body: JSON.stringify({ ssid: ssid, password: password })
  }).then(function (r) { return r.json(); }).then(report);
}

document.getElementById("join").addEventListener("submit", function (event) {
  event.preventDefault();
  var form = event.target;
  joinNetwork(form.ssid.value, form.password.value).catch(function () {
    showError("could not join network");
  });
});

document.getElementById("scan").addEventListener("click", function () {
  var button = document.getElementById("scan");
  var box = document.getElementById("networks");
  button.disabled = true;
  button.textContent = "Scanning…";
  fetch("/api/wifi/scan").then(function (r) { return r.json(); }).then(function (list) {
    box.textContent = "";
    list.forEach(function (item) {
      if (!item.ssid) return;
      var form = document.createElement("form");
      form.className = "row";
      var name = document.createElement("span");
      name.textContent = item.ssid;
      var signal = document.createElement("span");
      signal.className = "muted";
      signal.textContent = item.rssi + " dBm";
      var password = document.createElement("input");
      password.type = "password";
      password.placeholder = "password";
      password.autocomplete = "off";
      var submit = document.createElement("button");
      submit.type = "submit";
      submit.textContent = "Join";
      form.appendChild(name);
      form.appendChild(signal);
      form.appendChild(password);
      form.appendChild(submit);
      form.addEventListener("submit", function (event) {
        event.preventDefault();
        joinNetwork(item.ssid, password.value).catch(function () {
          showError("could not join network");
        });
      });
      box.appendChild(form);
    });
    if (!box.childNodes.length) {
      var empty = document.createElement("p");
      empty.className = "muted";
      empty.textContent = "No networks found.";
      box.appendChild(empty);
    }
  }).catch(function () {
    showError("scan failed");
  }).then(function () {
    button.disabled = false;
    button.textContent = "Scan networks";
  });
});

document.getElementById("upload").addEventListener("submit", function (event) {
  event.preventDefault();
  var form = event.target;
  fetch("/api/program/upload", {
    method: "POST",
    headers: { "Accept": "application/json" },
    body: new FormData(form)
  }).then(function (r) { return r.json(); }).then(function (data) {
    report(data);
    if (data.ok) form.reset();
  }).catch(function () {
    showError("upload failed");
  });
});

function refresh() {
  fetch("/api/status").then(function (r) { return r.json(); }).then(paintStatus).catch(function () {});
}
function refreshPreview() {
  fetch("/api/preview").then(function (r) { return r.json(); }).then(draw).catch(function () {});
}
loadLeds().catch(function () {});
loadPalette().catch(function () {});
loadPrograms().catch(function () {});
refresh();
refreshPreview();
setInterval(refresh, 2000);
setInterval(refreshPreview, 500);
</script>
</body>
</html>
)page";

static const char kRestartPage[] PROGMEM = R"page(<!doctype html>
<html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<meta http-equiv=refresh content="8;url=/">
<title>LEDBasic</title>
<style>
:root { color-scheme: light dark; }
body { font: 16px/1.45 system-ui, sans-serif; margin: 1.5rem; }
@media (prefers-color-scheme: dark) {
  body { background: #12110f; color: #ece8e1; }
}
</style></head>
<body><p>Restarting. Reconnect, then open this page again.</p></body></html>
)page";

static const char kNoticePrefix[] PROGMEM = R"page(<!doctype html>
<html><head><meta charset=utf-8>
<meta name=viewport content="width=device-width,initial-scale=1">
<title>LEDBasic</title>
<style>
:root { color-scheme: light dark; }
body { font: 16px/1.45 system-ui, sans-serif; margin: 1.5rem; }
a { color: inherit; }
@media (prefers-color-scheme: dark) {
  body { background: #12110f; color: #ece8e1; }
}
</style></head>
<body><p>)page";

static const char kNoticeSuffix[] PROGMEM = R"page(</p><p><a href="/">Back</a></p></body></html>)page";
