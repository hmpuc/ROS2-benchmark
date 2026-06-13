import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import glob
import os

def plot_summaries():
    # Pasta onde os arquivos summary_*.csv foram salvos
    input_dir = "./data/csv"
    output_base_dir = "./data/graph" # Base para as subpastas

    summary_files = glob.glob(os.path.join(input_dir, "summary_*.csv"))

    if not summary_files:
        print("Nenhum arquivo summary_*.csv encontrado.")
        return

    # Estilo do Seaborn
    sns.set_theme(style="whitegrid")

    for file in summary_files:
        b_name = os.path.basename(file).replace("summary_", "").replace(".csv", "")
        print(f"Gerando gráficos para: {b_name}")

        # Criar subpasta para o tipo de benchmark
        b_folder = os.path.join(output_base_dir, b_name)
        if not os.path.exists(b_folder):
            os.makedirs(b_folder)

        df = pd.read_csv(file)
        df = df.sort_values("sensor_count")

        # --- 1. Gráfico de Mensagens Recebidas ---
        plt.figure(figsize=(10, 6))
        sns.lineplot(data=df, x="sensor_count", y="received_mean", marker='o', label="Média Recebidos")
        plt.errorbar(df["sensor_count"], df["received_mean"], yerr=df["received_std"], fmt='none', capsize=5, ecolor='red', alpha=0.5)
        plt.title(f"Taxa de Entrega - {b_name}")
        plt.ylabel("Mensagens Recebidas")
        plt.xlabel("Número de Sensores")
        plt.xlim(0, 72)
        plt.ylim(0, 19000)
        plt.tight_layout()
        plt.savefig(os.path.join(b_folder, "received.png"), dpi=300)
        plt.close()

        # --- 2. Gráfico de Latência ---
        plt.figure(figsize=(10, 6))
        # Avg Latency
        sns.lineplot(data=df, x="sensor_count", y="avg_latency_ms_mean", marker='s', label="Latência Média", color='blue')
        plt.errorbar(df["sensor_count"], df["avg_latency_ms_mean"], yerr=df["avg_latency_ms_std"], 
                     fmt='none', capsize=3, ecolor='blue', alpha=0.4)
        
        # Max Latency
        sns.lineplot(data=df, x="sensor_count", y="max_latency_ms_mean", marker='^', label="Latência Máxima", color='orange')
        plt.errorbar(df["sensor_count"], df["max_latency_ms_mean"], yerr=df["max_latency_ms_std"], 
                     fmt='none', capsize=3, ecolor='orange', alpha=0.4)
        
        plt.title(f"Latência - {b_name}")
        plt.ylabel("Tempo (ms)")
        plt.xlabel("Número de Sensores")
        plt.xlim(0, 72)
        plt.ylim(0, 250)
        plt.legend()
        plt.tight_layout()
        plt.savefig(os.path.join(b_folder, "latency.png"), dpi=300)
        plt.close()

        # --- 3. Gráfico de Jitter ---
        plt.figure(figsize=(10, 6))
        # Avg Jitter
        sns.lineplot(data=df, x="sensor_count", y="avg_jitter_ms_mean", marker='d', label="Jitter Médio", color='green')
        plt.errorbar(df["sensor_count"], df["avg_jitter_ms_mean"], yerr=df["avg_jitter_ms_std"], 
                     fmt='none', capsize=3, ecolor='green', alpha=0.4)
        
        # Max Jitter
        sns.lineplot(data=df, x="sensor_count", y="max_jitter_ms_mean", marker='v', label="Jitter Máximo", color='red')
        plt.errorbar(df["sensor_count"], df["max_jitter_ms_mean"], yerr=df["max_jitter_ms_std"], 
                     fmt='none', capsize=3, ecolor='red', alpha=0.4)
        
        plt.title(f"Jitter - {b_name}")
        plt.ylabel("Tempo (ms)")
        plt.xlabel("Número de Sensores")
        plt.xlim(0, 72)
        plt.ylim(0, 250)
        plt.legend()
        plt.tight_layout()
        plt.savefig(os.path.join(b_folder, "jitter.png"), dpi=300)
        plt.close()

        print(f"Gráficos salvos em: {b_folder}")

if __name__ == "__main__":
    plot_summaries()
