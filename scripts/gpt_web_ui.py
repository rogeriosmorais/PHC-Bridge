import asyncio
import subprocess
import socket
import os
from playwright.async_api import async_playwright

# Configurações do seu ambiente
CHROME_PATH = r"C:\Program Files\Google\Chrome\Application\chrome.exe"
# DICA: Removi o "\Default" do final para o Chrome carregar seu perfil real corretamente
USER_DATA = r"C:\Users\roger\AppData\Local\Google\Chrome\User Data" 
PORT = 9222

def is_chrome_running():
    """Verifica se a porta de debug está aberta."""
    with socket.socket(socket.AF_INET, socket.SOCK_STREAM) as s:
        return s.connect_ex(('localhost', PORT)) == 0

async def interagir_chatgpt(conversacao_id, mensagem):
    # 1. Verifica se o Chrome está aberto no modo debug
    if not is_chrome_running():
        print("Chrome não detectado no modo debug. Iniciando...")
        comando = [
            CHROME_PATH,
            f"--remote-debugging-port={PORT}",
            f"--user-data-dir={USER_DATA}",
            "--profile-directory=Default" # Força o perfil Default
        ]
        subprocess.Popen(comando)
        # Espera o Chrome "acordar"
        await asyncio.sleep(5)
    else:
        print("Conectando ao Chrome já existente...")

    async with async_playwright() as p:
        try:
            browser = await p.chromium.connect_over_cdp(f"http://localhost:{PORT}")
            context = browser.contexts[0]
            
            target_url = f"https://chatgpt.com/c/{conversacao_id}"
            page = None

            # 2. Reutiliza a aba se já estiver aberta
            for p_aberta in context.pages:
                if conversacao_id in p_aberta.url:
                    page = p_aberta
                    break
            
            if not page:
                page = await context.new_page()
                await page.goto(target_url)

            await page.bring_to_front()

            # 3. Envio e Captura
            textarea = "#prompt-textarea"
            stop_button = 'button[data-testid="stop-button"]'
            
            await page.wait_for_selector(textarea)
            await page.fill(textarea, mensagem)
            await page.keyboard.press("Enter")
            print(f"Mensagem enviada: {mensagem}")

            # Espera a geração começar e terminar
            try:
                await page.wait_for_selector(stop_button, timeout=4000)
            except: pass

            await page.wait_for_selector(stop_button, state="hidden", timeout=90000)
            
            await asyncio.sleep(2)
            respostas = await page.locator(".markdown").all()
            if respostas:
                print("\n--- RESPOSTA ---\n", await respostas[-1].inner_text(), "\n----------------\n")

            await browser.close()

        except Exception as e:
            print(f"Erro na conexão ou interação: {e}")

# Execução
ID = "69b9831f-d740-8331-8b89-68e550f57ea3"
MSG = "Dê um outro exemplo de um comando git comum."
asyncio.run(interagir_chatgpt(ID, MSG))