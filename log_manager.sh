#!/bin/bash

ARQUIVO_ENTRADA="./log.csv"
QUANTIDADE_SENSORES="$2"
NUMERO_EXECUCAO="$3"

if [ -z "$QUANTIDADE_SENSORES" ] || [ -z "$NUMERO_EXECUCAO" ]; then
    echo "Uso: $0 <quantidade_sensores> <numero_execução>"
    exit 1
fi

PASTA_SAIDA="execLogs/$QUANTIDADE_SENSORES/$NUMERO_EXECUCAO"

mkdir -p "$PASTA_SAIDA"

while IFS= read -r linha
do
    [ -z "$linha" ] && continue

    prefixo=$(echo "$linha" | cut -d ';' -f1 | xargs)

    numero=$(echo "$prefixo" | sed -E 's/.*_x([0-9]+).*/\1/')

    arquivo_saida="$PASTA_SAIDA/$numero.log"

    # Adiciona linha ao arquivo correspondente
    echo "$linha" >> "$arquivo_saida"

done < "$ARQUIVO_ENTRADA"

echo "Logs separados em: $PASTA_SAIDA"