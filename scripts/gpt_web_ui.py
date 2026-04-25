import asyncio
import subprocess
import socket
import os
import sys
from playwright.async_api import async_playwright

sys.path.append(os.getcwd())
from scripts.read_logs import extract_physa_to_clipboard

CHROME_CMD = [
    r"C:\Program Files\Google\Chrome\Application\chrome.exe",
    "--remote-debugging-port=9222",
    r"--user-data-dir=C:\Users\roger\AppData\Local\Google\Chrome\User Data\Default"
]
CONVERSA_ID = "69b76e88-63d8-8328-b269-763a6c67b212"

def is_chrome_open():
    """Checks if port 9222 is responding."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        return s.connect_ex(('localhost', 9222)) == 0

async def run():    
    print("🔍 Extracting log data...")
    conteudo_log = extract_physa_to_clipboard()
    if not conteudo_log:
        print("❌ Empty log, stopping.")
        return
    
    if not is_chrome_open():
        print("🚀 Opening Chrome...")
        subprocess.Popen(CHROME_CMD)
        await asyncio.sleep(5)

    async with async_playwright() as p:
        # 3. Connect
        browser = await p.chromium.connect_over_cdp("http://localhost:9222")
        context = browser.contexts[0]
        
        # 4. Look for the tab or open a new one
        target_url = f"https://chatgpt.com/c/{CONVERSA_ID}"
        page = next((p for p in context.pages if CONVERSA_ID in p.url), None)
        
        if not page:
            page = await context.new_page()
            await page.goto(target_url)
        
        await page.bring_to_front()

        # 5. Inject the log (using evaluate to avoid the Timeout that occurred before)
        textarea = "#prompt-textarea"
        await page.wait_for_selector(textarea)
        
        print("📤 Posting log...")
        await page.locator(textarea).evaluate("""
            (el, text) => {
                el.innerText = text;
                el.dispatchEvent(new Event('input', { bubbles: true }));
            }
        """, conteudo_log)

        await asyncio.sleep(1)
        await page.keyboard.press("Enter")

        # 6. Wait and get the response
        print("⏳ Waiting for response...")
        stop_btn = 'button[data-testid="stop-button"]'
        
        try:
            # Wait for the stop button to appear and then disappear
            await page.wait_for_selector(stop_btn, timeout=5000)
            await page.wait_for_selector(stop_btn, state="hidden", timeout=120000)
        except:
            pass # Response was very fast or the selector changed

        await asyncio.sleep(2)
        respostas = await page.locator(".markdown").all()
        if respostas:
            print("\n--- RESPONSE ---\n", await respostas[-1].inner_text(), "\n----------------")
        
        await browser.close()

if __name__ == "__main__":
    asyncio.run(run())