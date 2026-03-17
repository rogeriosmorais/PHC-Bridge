import asyncio
from playwright.async_api import async_playwright

async def interagir_chatgpt(conversacao_id, mensagem):
    async with async_playwright() as p:
        # Conecta ao Chrome já aberto
        browser = await p.chromium.connect_over_cdp("http://localhost:9222")
        context = browser.contexts[0]
        
        target_url = f"https://chatgpt.com/c/{conversacao_id}"
        page = None

        # Procura se a aba já existe
        for p_aberta in context.pages:
            if conversacao_id in p_aberta.url:
                page = p_aberta
                print("Reutilizando aba aberta...")
                break
        
        if not page:
            print("Abrindo nova aba...")
            page = await context.new_page()
            await page.goto(target_url)

        await page.bring_to_front()

        # Seletores
        textarea = "#prompt-textarea"
        stop_button = 'button[data-testid="stop-button"]'
        
        try:
            await page.wait_for_selector(textarea)
            await page.fill(textarea, mensagem)
            await page.keyboard.press("Enter")
            print("Mensagem enviada!")

            # Lógica de espera do botão de Stop
            try:
                await page.wait_for_selector(stop_button, timeout=4000)
            except: pass

            await page.wait_for_selector(stop_button, state="hidden", timeout=90000)
            print("Geração finalizada.")

            await asyncio.sleep(2)

            respostas = await page.locator(".markdown").all()
            if respostas:
                ultima_resposta = await respostas[-1].inner_text()
                print("\n--- RESPOSTA ---\n" + ultima_resposta + "\n----------------\n")
            
        except Exception as e:
            print(f"Erro: {e}")
        
        # O SEGREDO AQUI: 
        # Isso fecha a sessão do Playwright, mas o Chrome continua rodando.
        await browser.close()

# Execução
ID = "69b9831f-d740-8331-8b89-68e550f57ea3"
MSG = "Dê um exemplo de um comando git comum."
asyncio.run(interagir_chatgpt(ID, MSG))