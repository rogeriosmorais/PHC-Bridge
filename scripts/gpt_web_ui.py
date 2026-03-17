import asyncio
import subprocess
import socket
import os
import sys
from playwright.async_api import async_playwright

# Import da sua função
sys.path.append(os.getcwd())
from scripts.read_logs import extract_physa_to_clipboard

# Configurações exatas que você passou
CHROME_CMD = [
    r"C:\Program Files\Google\Chrome\Application\chrome.exe",
    "--remote-debugging-port=9222",
    r"--user-data-dir=C:\Users\roger\AppData\Local\Google\Chrome\User Data\Default"
]
CONVERSA_ID = "69b76e88-63d8-8328-b269-763a6c67b212"

def chrome_esta_aberto():
    """Checa se a porta 9222 está respondendo."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        return s.connect_ex(('localhost', 9222)) == 0

async def executar():
    # 1. Extrai o log primeiro
    print("🔍 Extraindo dados do log...")
    conteudo_log = extract_physa_to_clipboard()
    if not conteudo_log:
        print("❌ Log vazio, parando.")
        return

    # 2. Abre o Chrome se não estiver aberto no modo debug
    if not chrome_esta_aberto():
        print("🚀 Abrindo Chrome...")
        subprocess.Popen(CHROME_CMD)
        await asyncio.sleep(5)

    async with async_playwright() as p:
        # 3. Conecta
        browser = await p.chromium.connect_over_cdp("http://localhost:9222")
        context = browser.contexts[0]
        
        # 4. Procura a aba ou abre nova
        target_url = f"https://chatgpt.com/c/{CONVERSA_ID}"
        page = next((p for p in context.pages if CONVERSA_ID in p.url), None)
        
        if not page:
            page = await context.new_page()
            await page.goto(target_url)
        
        await page.bring_to_front()

        # 5. Injeta o log (usando evaluate para não dar o Timeout que deu antes)
        textarea = "#prompt-textarea"
        await page.wait_for_selector(textarea)
        
        print("📤 Postando log...")
        await page.locator(textarea).evaluate("""
            (el, text) => {
                el.innerText = text;
                el.dispatchEvent(new Event('input', { bubbles: true }));
            }
        """, conteudo_log)

        await asyncio.sleep(1)
        await page.keyboard.press("Enter")

        # 6. Espera e pega a resposta
        print("⏳ Aguardando resposta...")
        stop_btn = 'button[data-testid="stop-button"]'
        
        try:
            # Espera o botão de stop aparecer e depois sumir
            await page.wait_for_selector(stop_btn, timeout=5000)
            await page.wait_for_selector(stop_btn, state="hidden", timeout=120000)
        except:
            pass # Resposta foi muito rápida ou o seletor mudou

        await asyncio.sleep(2)
        respostas = await page.locator(".markdown").all()
        if respostas:
            print("\n--- RESPOSTA ---\n", await respostas[-1].inner_text(), "\n----------------")
        
        await browser.close()

if __name__ == "__main__":
    asyncio.run(executar())