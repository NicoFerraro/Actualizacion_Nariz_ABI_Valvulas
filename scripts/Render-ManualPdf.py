import argparse
import base64
import json
import shutil
import subprocess
import sys
import tempfile
import time
import urllib.parse
import urllib.request
from pathlib import Path


def ensure_websocket_client():
    try:
        import websocket  # type: ignore
        return websocket
    except ImportError:
        subprocess.check_call([sys.executable, "-m", "pip", "install", "websocket-client"])
        import websocket  # type: ignore
        return websocket


def wait_for_debug_endpoint(port: int, timeout: float = 15.0):
    deadline = time.time() + timeout
    last_error = None
    while time.time() < deadline:
        try:
            with urllib.request.urlopen(f"http://127.0.0.1:{port}/json/version", timeout=2) as response:
                return json.loads(response.read().decode("utf-8"))
        except Exception as exc:  # noqa: BLE001
            last_error = exc
            time.sleep(0.2)
    raise RuntimeError(f"No se pudo abrir el endpoint de depuracion del navegador: {last_error}")


def create_target(port: int, target_url: str):
    encoded_url = urllib.parse.quote(target_url, safe="")
    request = urllib.request.Request(
        f"http://127.0.0.1:{port}/json/new?{encoded_url}",
        method="PUT",
    )
    with urllib.request.urlopen(request, timeout=10) as response:
        return json.loads(response.read().decode("utf-8"))


class CdpClient:
    def __init__(self, websocket_module, websocket_url: str):
        self._websocket = websocket_module.create_connection(websocket_url, timeout=20)
        self._next_id = 1

    def send(self, method: str, params: dict | None = None):
        message_id = self._next_id
        self._next_id += 1
        payload = {"id": message_id, "method": method}
        if params:
            payload["params"] = params
        self._websocket.send(json.dumps(payload))

        while True:
            raw = self._websocket.recv()
            message = json.loads(raw)
            if message.get("id") == message_id:
                if "error" in message:
                    raise RuntimeError(f"CDP {method} fallo: {message['error']}")
                return message.get("result", {})

    def close(self):
        try:
            self._websocket.close()
        except Exception:  # noqa: BLE001
            pass


def wait_until_ready(client: CdpClient, timeout: float = 20.0):
    expression = """
(() => {
  const imagesReady = Array.from(document.images || []).every((img) => img.complete);
  return document.readyState === 'complete' && imagesReady;
})()
""".strip()
    deadline = time.time() + timeout
    while time.time() < deadline:
        result = client.send(
            "Runtime.evaluate",
            {
                "expression": expression,
                "returnByValue": True,
            },
        )
        if result.get("result", {}).get("value") is True:
            return
        time.sleep(0.25)
    raise RuntimeError("La pagina no termino de cargar a tiempo.")


def render_pdf(client: CdpClient, pdf_path: Path):
    header_template = """
<div style="width:100%; font-size:8px; color:#4a6271; font-family:'Segoe UI', Arial, sans-serif; padding:0 14px; box-sizing:border-box;">
  <div style="border-bottom:1px solid #d7e1e8; padding-bottom:2px;">Manual de Usuario - Nariz Metatron</div>
</div>
""".strip()

    footer_template = """
<div style="width:100%; font-size:8px; color:#4a6271; font-family:'Segoe UI', Arial, sans-serif; padding:0 14px; box-sizing:border-box;">
  <div style="border-top:1px solid #d7e1e8; padding-top:2px;">
    <table style="width:100%; border-collapse:collapse;">
      <tr>
        <td style="text-align:left;">Einsted S.A.</td>
        <td style="text-align:right;">Pagina <span class="pageNumber"></span></td>
      </tr>
    </table>
  </div>
</div>
""".strip()

    result = client.send(
        "Page.printToPDF",
        {
            "landscape": False,
            "printBackground": True,
            "preferCSSPageSize": False,
            "paperWidth": 8.2677,
            "paperHeight": 11.6929,
            "marginTop": 0.7874,
            "marginBottom": 0.7874,
            "marginLeft": 1.1811,
            "marginRight": 0.5906,
            "displayHeaderFooter": True,
            "headerTemplate": header_template,
            "footerTemplate": footer_template,
        },
    )

    pdf_path.write_bytes(base64.b64decode(result["data"]))


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("--browser", required=True)
    parser.add_argument("--html", required=True)
    parser.add_argument("--pdf", required=True)
    args = parser.parse_args()

    websocket_module = ensure_websocket_client()

    browser_path = Path(args.browser)
    html_path = Path(args.html).resolve()
    pdf_path = Path(args.pdf).resolve()
    pdf_path.parent.mkdir(parents=True, exist_ok=True)

    html_uri = html_path.as_uri()
    port = 9222
    user_data_dir = Path(tempfile.mkdtemp(prefix="nariz-metatron-pdf-"))

    process = subprocess.Popen(
        [
            str(browser_path),
            "--headless=new",
            "--disable-gpu",
            "--remote-allow-origins=*",
            f"--remote-debugging-port={port}",
            f"--user-data-dir={user_data_dir}",
            "--no-first-run",
            "--no-default-browser-check",
            "about:blank",
        ],
        stdout=subprocess.DEVNULL,
        stderr=subprocess.DEVNULL,
    )

    client = None
    try:
        wait_for_debug_endpoint(port)
        target = create_target(port, html_uri)
        client = CdpClient(websocket_module, target["webSocketDebuggerUrl"])
        client.send("Page.enable")
        client.send("Runtime.enable")
        client.send("Emulation.setEmulatedMedia", {"media": "print"})
        wait_until_ready(client)
        render_pdf(client, pdf_path)
    finally:
        if client is not None:
            client.close()
        process.terminate()
        try:
            process.wait(timeout=5)
        except subprocess.TimeoutExpired:
            process.kill()
        shutil.rmtree(user_data_dir, ignore_errors=True)


if __name__ == "__main__":
    main()
