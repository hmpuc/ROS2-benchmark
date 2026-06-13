import pandas as pd
import matplotlib.pyplot as plt
import seaborn as sns
import glob
import os

def plot_comparison():
    input_dir = "./data/csv"
    output_base_dir = "./data/graph_comparison"
    
    summary_files = glob.glob(os.path.join(input_dir, "summary_*.csv"))
    
    if not summary_files:
        print("Nenhum arquivo summary_*.csv encontrado.")
        return

    # Estilo do Seaborn
    sns.set_theme(style="whitegrid")
    
    # Categorias solicitadas
    categories = {
        "mtu9000": [],
        "network": [],
        "base": []
    }

    # Organizar arquivos por categoria
    for file in summary_files:
        b_name = os.path.basename(file).replace("summary_", "").replace(".csv", "")
        if "mtu9000" in b_name:
            categories["mtu9000"].append((b_name, file))
        elif "network" in b_name:
            categories["network"].append((b_name, file))
        else:
            categories["base"].append((b_name, file))

    for cat_name, files in categories.items():
        if not files:
            continue
            
        print(f"Gerando comparações para categoria: {cat_name}")
        
        # Pasta para a categoria
        cat_folder = os.path.join(output_base_dir, cat_name)
        if not os.path.exists(cat_folder):
            os.makedirs(cat_folder)

        # Carregar dados da categoria
        cat_data = []
        for b_name, file in files:
            df = pd.read_csv(file)
            df["benchmark_type"] = b_name
            cat_data.append(df)
        
        combined_df = pd.concat(cat_data, ignore_index=True)
        combined_df = combined_df.sort_values(["benchmark_type", "sensor_count"])

        # --- 1. Comparação de Mensagens Recebidas ---
        plt.figure(figsize=(12, 7))
        sns.lineplot(data=combined_df, x="sensor_count", y="received_mean", hue="benchmark_type", marker='o')
        plt.title(f"Comparação Taxa de Entrega - {cat_name.upper()}")
        plt.ylabel("Mensagens Recebidas (Média)")
        plt.xlabel("Número de Sensores")
        plt.xlim(0, 72)
        plt.ylim(0, 19000)
        plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
        plt.tight_layout()
        plt.savefig(os.path.join(cat_folder, f"comparison_{cat_name}_received.png"), dpi=300)
        plt.close()

        # --- 2. Comparação de Latência Média ---
        plt.figure(figsize=(12, 7))
        sns.lineplot(data=combined_df, x="sensor_count", y="avg_latency_ms_mean", hue="benchmark_type", marker='s')
        plt.title(f"Comparação Latência Média - {cat_name.upper()}")
        plt.ylabel("Tempo (ms)")
        plt.xlabel("Número de Sensores")
        plt.xlim(0, 72)
        plt.ylim(0, 250)
        plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
        plt.tight_layout()
        plt.savefig(os.path.join(cat_folder, f"comparison_{cat_name}_latency_avg.png"), dpi=300)
        plt.close()

        # --- 3. Comparação de Jitter Médio ---
        plt.figure(figsize=(12, 7))
        sns.lineplot(data=combined_df, x="sensor_count", y="avg_jitter_ms_mean", hue="benchmark_type", marker='d')
        plt.title(f"Comparação Jitter Médio - {cat_name.upper()}")
        plt.ylabel("Tempo (ms)")
        plt.xlabel("Número de Sensores")
        plt.xlim(0, 72)
        plt.ylim(0, 250)
        plt.legend(bbox_to_anchor=(1.05, 1), loc='upper left')
        plt.tight_layout()
        plt.savefig(os.path.join(cat_folder, f"comparison_{cat_name}_jitter_avg.png"), dpi=300)
        plt.close()

    print(f"Gráficos de comparação categorizados salvos em: {output_base_dir}")

if __name__ == "__main__":
    plot_comparison()
