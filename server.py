"""
GEX-ES 本地开发服务器
- 提供静态文件服务 (demo.html 等)
- 代理 /api/yahoo/* 请求到 Yahoo Finance (绕过浏览器 CORS 限制)
"""
import http.server
import urllib.request
import urllib.error
import json
import os
import sys
from datetime import datetime, timedelta

PORT = 8080
YAHOO_BASE = "https://query1.finance.yahoo.com"
YAHOO_CHART = "https://query2.finance.yahoo.com"


class ProxyHandler(http.server.SimpleHTTPRequestHandler):
    """同时处理静态文件 + Yahoo Finance API 代理"""

    def do_GET(self):
        # ── Yahoo Finance API 代理 ──────────────────────
        if self.path.startswith("/api/yahoo/"):
            return self._proxy_yahoo()

        # ── SPY / ES 现价快捷接口 ───────────────────────
        if self.path == "/api/spot":
            return self._fetch_spot_prices()

        # ── 静态文件 ─────────────────────────────────────
        return super().do_GET()

    def _proxy_yahoo(self):
        """将 /api/yahoo/v8/finance/chart/SPY → Yahoo Finance"""
        yahoo_path = self.path[len("/api/yahoo"):]
        yahoo_url = YAHOO_BASE + yahoo_path

        try:
            req = urllib.request.Request(yahoo_url)
            req.add_header("User-Agent",
                           "Mozilla/5.0 (Windows NT 10.0; Win64; x64) "
                           "AppleWebKit/537.36")
            with urllib.request.urlopen(req, timeout=10) as resp:
                data = resp.read()
                self.send_response(200)
                self.send_header("Content-Type", "application/json")
                self.send_header("Access-Control-Allow-Origin", "*")
                self.send_header("Cache-Control", "max-age=30")
                self.end_headers()
                self.wfile.write(data)
        except urllib.error.HTTPError as e:
            self._error(e.code, f"Yahoo API HTTP {e.code}")
        except Exception as e:
            self._error(502, f"代理请求失败: {str(e)}")

    @staticmethod
    def _get_es_contract():
        """根据当前日期计算 ES 主力合约代码 (如 ESU26.CME)"""
        # ES 期货季月代码: H=3月, M=6月, U=9月, Z=12月
        months = [(3, 'H'), (6, 'M'), (9, 'U'), (12, 'Z')]
        now = datetime.now()
        yr = now.year % 100  # 年份后两位

        for m, code in months:
            expiry = datetime(now.year, m, 20)
            if now < expiry:
                return f"ES{code}{yr:02d}.CME"

        # 12月已过 → 次年3月
        return f"ESH{(yr + 1) % 100:02d}.CME"

    def _fetch_spot_prices(self):
        """获取 SPY 现价和 ES 现价"""
        result = {"spySpot": None, "esPrice": None}

        # 获取 SPY 现价
        try:
            url = f"{YAHOO_CHART}/v8/finance/chart/SPY?interval=1d&range=1d"
            req = urllib.request.Request(url)
            req.add_header("User-Agent",
                           "Mozilla/5.0 (Windows NT 10.0; Win64; x64)")
            with urllib.request.urlopen(req, timeout=10) as resp:
                data = json.loads(resp.read())
                result["spySpot"] = data["chart"]["result"][0]["meta"][
                    "regularMarketPrice"]
        except Exception as e:
            print(f"[proxy] SPY spot fetch failed: {e}", file=sys.stderr)

        # 获取 ES 现价 (优先主力合约，回退连续合约)
        es_contract = self._get_es_contract()
        for symbol in [es_contract, "ES=F"]:
            try:
                url = f"{YAHOO_CHART}/v8/finance/chart/{symbol}?interval=1d&range=1d"
                req = urllib.request.Request(url)
                req.add_header("User-Agent",
                               "Mozilla/5.0 (Windows NT 10.0; Win64; x64)")
                with urllib.request.urlopen(req, timeout=10) as resp:
                    data = json.loads(resp.read())
                    result["esPrice"] = data["chart"]["result"][0]["meta"][
                        "regularMarketPrice"]
                    result["esContract"] = symbol
                    break
            except Exception as e:
                print(f"[proxy] {symbol} fetch failed: {e}", file=sys.stderr)

        # 返回 JSON
        body = json.dumps(result).encode("utf-8")
        self.send_response(200)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.send_header("Cache-Control", "max-age=5")
        self.end_headers()
        self.wfile.write(body)

    def _error(self, code, message):
        body = json.dumps({"error": message}).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json")
        self.send_header("Access-Control-Allow-Origin", "*")
        self.end_headers()
        self.wfile.write(body)

    def log_message(self, format, *args):
        # 简洁日志
        if "/api/" in str(args[0]):
            print(f"[proxy] {args[0]}", file=sys.stderr)
        else:
            super().log_message(format, *args)


if __name__ == "__main__":
    os.chdir(os.path.dirname(os.path.abspath(__file__)))
    print(f"\n  GEX-ES 本地服务器已启动")
    print(f"  地址: http://localhost:{PORT}/demo.html")
    print(f"  API:  http://localhost:{PORT}/api/yahoo/* → Yahoo Finance")
    print(f"  API:  http://localhost:{PORT}/api/spot     → SPY/ES 现价\n")
    http.server.HTTPServer(("", PORT), ProxyHandler).serve_forever()
