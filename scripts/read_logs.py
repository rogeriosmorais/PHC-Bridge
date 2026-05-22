import os
import glob
import pyperclip

SCRIPT_DIR = os.path.dirname(os.path.abspath(__file__))
REPO_ROOT = os.path.abspath(os.path.join(SCRIPT_DIR, os.pardir))
DEFAULT_LOG_DIR = os.path.join(REPO_ROOT, "PhysAnimUE5", "Saved", "Logs")

def extract_physa_to_clipboard(folder_path=DEFAULT_LOG_DIR):
    # 1. Busca todos os arquivos .log na pasta
    # Você pode mudar a extensão para '*' se os arquivos não tiverem extensão fixa
    files = glob.glob(os.path.join(folder_path, "*.log"))

    if not files:
        print("Nenhum arquivo de log encontrado.")
        return

    # 2. Encontra o arquivo mais recente baseado no tempo de modificação
    latest_file = max(files, key=os.path.getmtime)
    print(f"Lendo o arquivo mais recente: {latest_file}")

    # 3. Filtra as linhas que contêm 'PhysA'
    extracted_lines = []
    try:
        with open(latest_file, 'r', encoding='utf-8') as file:
            for line in file:
                if "PhysA" in line or "PROOFIX" in line:
                    extracted_lines.append(line.strip())
    except Exception as e:
        print(f"Erro ao ler o arquivo: {e}")
        return

    # 4. Junta as linhas e copia para a área de transferência
    if extracted_lines:
        content_to_copy = "\n".join(extracted_lines)
        pyperclip.copy(content_to_copy)
        print(f"Sucesso! {len(extracted_lines)} linhas copiadas para o clipboard.")
        return content_to_copy
    else:
        print("Nenhuma linha com 'PhysA' foi encontrada.")

# Exemplo de uso:
# Use '.' para a pasta atual ou o caminho completo como 'C:/Logs/'
caminho_da_pasta = DEFAULT_LOG_DIR
print(extract_physa_to_clipboard(caminho_da_pasta))
