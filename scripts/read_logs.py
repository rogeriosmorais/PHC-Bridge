import os
import glob
import pyperclip

import physanim_logger as logger

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir))
DEFAULT_LOG_DIR = os.path.join(REPO_ROOT, "PhysAnimUE5", "Saved", "Logs")

def extract_physa_to_clipboard(folder_path=DEFAULT_LOG_DIR):
    # 1. Busca todos os arquivos .log na pasta
    # Você pode mudar a extensão para '*' se os arquivos não tiverem extensão fixa
    files = glob.glob(os.path.join(folder_path, "*.log"))

    if not files:
        logger.warning("Nenhum arquivo de log encontrado.")
        return

    # 2. Encontra o arquivo mais recente baseado no tempo de modificação
    latest_file = max(files, key=os.path.getmtime)
    logger.info("Lendo o arquivo mais recente: %s", 0.0, latest_file)

    # 3. Filtra as linhas que contêm 'PhysA'
    extracted_lines = []
    try:
        with open(latest_file, 'r', encoding='utf-8') as file:
            for line in file:
                if "PhysA" in line or "PROOFIX" in line:
                    extracted_lines.append(line.strip())
    except Exception as e:
        logger.error("Erro ao ler o arquivo: %s", 0.0, e)
        return

    # 4. Junta as linhas e copia para a área de transferência
    if extracted_lines:
        content_to_copy = "\n".join(extracted_lines)
        pyperclip.copy(content_to_copy)
        logger.info("Sucesso! %d linhas copiadas para o clipboard.", 0.0, len(extracted_lines))
        return content_to_copy
    else:
        logger.info("Nenhuma linha com 'PhysA' foi encontrada.")

# Exemplo de uso:
# Use '.' para a pasta atual ou o caminho completo como 'C:/Logs/'
caminho_da_pasta = DEFAULT_LOG_DIR
logger.info(extract_physa_to_clipboard(caminho_da_pasta))
