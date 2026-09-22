#pragma once

// Phone editor served at /ledbasic. Same origin as the device, so it can call
// /json/state without crossing origins. No external scripts.
static const char LEDBASIC_PAGE[] PROGMEM = R"LEDHTML(<!DOCTYPE html>
<html lang="en">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>LEDBasic</title>
<style>
  :root { color-scheme: dark; }
  body { margin: 0; font-family: ui-sans-serif, system-ui, sans-serif; background: #12141a; color: #e6e8ee; }
  header, #params { display: flex; flex-wrap: wrap; gap: 8px; padding: 8px; align-items: center; }
  header { background: #1c2030; position: sticky; top: 0; }
  select, button, input, textarea { font: 16px/1.3 ui-monospace, monospace; }
  button, select { background: #2a3148; color: #e6e8ee; border: 0; border-radius: 8px; padding: 10px 12px; }
  button { background: #2f6f66; }
  canvas { width: 100%; height: 56px; background: #000; display: block; }
  #err { min-height: 1.3em; padding: 6px 10px; color: #f0a0a0; }
  textarea { width: 100%; height: 46vh; box-sizing: border-box; background: #0c0e14; color: #d7dbe6; border: 0; padding: 10px; }
  label { display: flex; gap: 8px; align-items: center; background: #1c2030; border-radius: 8px; padding: 6px 8px; }
  input[type="range"] { width: 140px; }
</style>
</head>
<body>
<header>
  <select id="list" aria-label="Scripts"></select>
  <button id="run" type="button">Run</button>
  <button id="save" type="button">Save</button>
  <button id="neu" type="button">New</button>
  <button id="dup" type="button">Duplicate</button>
  <button id="del" type="button">Delete</button>
</header>
<canvas id="cv" height="56"></canvas>
<div id="err"></div>
<div id="params"></div>
<textarea id="src" spellcheck="false" aria-label="Script"></textarea>
<script>
const $ = (id) => document.getElementById(id);
let programs = [];
let msgUntil = 0;
let paramSig = "";
let holdParams = 0;
let draft = false;

function say(text) {
  $("err").textContent = text || "";
  msgUntil = Date.now() + 2200;
}

async function refresh(selectName) {
  const response = await fetch("/ledbasic/programs");
  const body = await response.json();
  programs = body.programs || [];
  const list = $("list");
  const previous = selectName || list.value || body.active || "";
  list.innerHTML = "";
  for (const program of programs) {
    const option = document.createElement("option");
    option.value = program.name;
    option.textContent = program.name + (program.origin === "builtin" ? " (built-in)" : "");
    list.appendChild(option);
  }
  if ([...list.options].some((option) => option.value === previous)) list.value = previous;
}

function selected() {
  return programs.find((program) => program.name === $("list").value);
}

async function loadSource(name) {
  if (!name) return;
  const response = await fetch("/ledbasic/program?name=" + encodeURIComponent(name));
  if (!response.ok) { say("Could not load " + name); return; }
  $("src").value = await response.text();
}

function askName(label, seed) {
  const name = prompt(label, seed || "MyEffect");
  if (!name) return "";
  return name.trim();
}

$("list").onchange = () => { draft = false; loadSource($("list").value); };

$("run").onclick = async () => {
  const name = $("list").value;
  if (!name) return;
  const response = await fetch("/json/state", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify({ LEDBasic: { programName: name } })
  });
  if (!response.ok) { say("Could not run " + name); return; }
  const listed = await (await fetch("/ledbasic/programs")).json();
  say(listed.active === name ? ("Running " + name) : ("Could not run " + name));
  await refresh(name);
};

$("save").onclick = async () => {
  let name = $("list").value;
  const current = selected();
  if (draft || !current || current.origin === "builtin") {
    name = askName("Save as (letters, digits, underscore)", current ? current.name + "Copy" : "MyEffect");
    if (!name) return;
  }
  const response = await fetch("/ledbasic/program?name=" + encodeURIComponent(name), {
    method: "PUT",
    body: $("src").value
  });
  if (!response.ok) { say(await response.text()); return; }
  say("Saved " + name);
  draft = false;
  await refresh(name);
  await loadSource(name);
};

$("neu").onclick = () => {
  draft = true;
  $("src").value = "setup\n  clear()\nend\n\nloop(time)\n  show()\nend\n";
  say("New script. Save it under a new name.");
};

$("dup").onclick = async () => {
  const name = askName("Duplicate as", ($("list").value || "Effect") + "Copy");
  if (!name) return;
  const response = await fetch("/ledbasic/program?name=" + encodeURIComponent(name), {
    method: "PUT",
    body: $("src").value
  });
  if (!response.ok) { say(await response.text()); return; }
  say("Saved " + name);
  await refresh(name);
};

$("del").onclick = async () => {
  const name = $("list").value;
  const current = selected();
  if (!current || current.origin !== "user") { say("Built-in scripts stay on the device."); return; }
  if (!confirm("Delete " + name + "?")) return;
  const response = await fetch("/ledbasic/program?name=" + encodeURIComponent(name), { method: "DELETE" });
  if (!response.ok) { say(await response.text()); return; }
  say("Deleted " + name);
  await refresh();
  if ($("list").value) await loadSource($("list").value);
};

function bytesFromBase64(text) {
  const binary = atob(text || "");
  const out = new Uint8Array(binary.length);
  for (let i = 0; i < binary.length; i++) out[i] = binary.charCodeAt(i);
  return out;
}

function draw(frame) {
  const canvas = $("cv");
  const width = canvas.clientWidth || 320;
  if (canvas.width !== width) canvas.width = width;
  const ctx = canvas.getContext("2d");
  ctx.fillStyle = "#000";
  ctx.fillRect(0, 0, width, canvas.height);
  const count = frame.n || 0;
  if (!count || !frame.rgb) return;
  const pixels = bytesFromBase64(frame.rgb);
  const cell = width / count;
  const scale = (frame.bri == null ? 255 : frame.bri) / 255;
  for (let i = 0; i < count; i++) {
    const red = (pixels[i * 3] || 0) * scale;
    const green = (pixels[i * 3 + 1] || 0) * scale;
    const blue = (pixels[i * 3 + 2] || 0) * scale;
    ctx.fillStyle = "rgb(" + (red | 0) + "," + (green | 0) + "," + (blue | 0) + ")";
    ctx.fillRect(Math.floor(i * cell), 0, Math.ceil(cell) + 1, canvas.height);
  }
}

function paintParams(params) {
  const list = params || [];
  const sig = list.map((param) => param.name + ":" + param.t).join(",");
  const box = $("params");
  if (sig !== paramSig) {
    paramSig = sig;
    box.innerHTML = "";
    for (const param of list) {
      const label = document.createElement("label");
      label.textContent = param.name;
      let input;
      if (param.t === 0) {
        input = document.createElement("input");
        input.type = "checkbox";
        input.checked = Number(param.v) !== 0;
        input.onchange = () => {
          holdParams = Date.now() + 900;
          postParam(param.name, input.checked ? 1 : 0);
        };
      } else {
        input = document.createElement("input");
        input.type = "range";
        input.min = String(param.min);
        input.max = String(param.max);
        const step = Number(param.step) > 0 ? Number(param.step) : 1;
        input.step = String(step);
        input.value = String(param.v);
        const value = document.createElement("span");
        value.textContent = String(param.v);
        input.oninput = () => {
          value.textContent = input.value;
          holdParams = Date.now() + 900;
          postParam(param.name, Number(input.value));
        };
        label.appendChild(input);
        label.appendChild(value);
        input.dataset.name = param.name;
        box.appendChild(label);
        continue;
      }
      input.dataset.name = param.name;
      label.appendChild(input);
      box.appendChild(label);
    }
    return;
  }
  if (Date.now() < holdParams) return;
  for (const param of list) {
    const input = box.querySelector("[data-name='" + param.name + "']");
    if (!input || document.activeElement === input) continue;
    if (input.type === "checkbox") input.checked = Number(param.v) !== 0;
    else if (document.activeElement !== input && input.value !== String(param.v)) {
      input.value = String(param.v);
      if (input.nextElementSibling) input.nextElementSibling.textContent = String(param.v);
    }
  }
}

function postParam(name, value) {
  const body = { LEDBasic: { params: {} } };
  body.LEDBasic.params[name] = value;
  fetch("/json/state", {
    method: "POST",
    headers: { "Content-Type": "application/json" },
    body: JSON.stringify(body)
  });
}

async function frame() {
  try {
    const response = await fetch("/ledbasic/frame");
    if (!response.ok) return;
    const body = await response.json();
    draw(body);
    paintParams(body.params);
    if (Date.now() > msgUntil) {
      $("err").textContent = body.error ? ((body.program || "LEDBasic") + ": " + body.error) : "";
    }
  } catch (err) {}
}

refresh().then(() => loadSource($("list").value)).catch((err) => say(String(err)));
setInterval(() => { if (!document.hidden) frame(); }, 200);
</script>
</body>
</html>
)LEDHTML";
