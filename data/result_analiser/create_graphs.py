import os
import pandas as pd
import glob
import re

def process_benchmarks():
    results_dir = "./results"
    output_dir = "./data/csv"
    
    # Get all subdirectories in results/
    benchmark_types = [d for d in os.listdir(results_dir) if os.path.isdir(os.path.join(results_dir, d))]
    
    metrics = ["received", "avg_latency_ms", "max_latency_ms", "avg_jitter_ms", "max_jitter_ms"]
    
    for b_type in benchmark_types:
        benchmarks_path = os.path.join(results_dir, b_type, "benchmarks")
        if not os.path.exists(benchmarks_path):
            print(f"Skipping {b_type}: 'benchmarks' folder not found.")
            continue
            
        print(f"Processing benchmark type: {b_type}")
        
        # Dictionary to store results per sensor_count
        # sensor_count -> list of means (one mean per test run)
        sensor_data = {}
        
        # Pattern to match benchmark_results_<sensor_count>_<test_run>.csv
        csv_pattern = os.path.join(benchmarks_path, "benchmark_results_*_*.csv")
        csv_files = glob.glob(csv_pattern)
        
        for csv_file in csv_files:
            # Extract sensor_count and test_run from filename
            match = re.search(r"benchmark_results_(\d+)_(\d+)\.csv", os.path.basename(csv_file))
            if not match:
                continue
                
            sensor_count = int(match.group(1))
            test_run = int(match.group(2))
            
            try:
                # Load CSV with semicolon separator
                df = pd.read_csv(csv_file, sep=';')
                
                # Calculate mean for this specific test run across all sensors
                run_means = df[metrics].mean()
                
                if sensor_count not in sensor_data:
                    sensor_data[sensor_count] = []
                
                sensor_data[sensor_count].append(run_means)
                
            except Exception as e:
                print(f"Error processing {csv_file}: {e}")

        if not sensor_data:
            print(f"No valid data found for {b_type}")
            continue

        # Aggregate data across test runs for each sensor_count
        summary_results = []
        
        for sensor_count in sorted(sensor_data.keys()):
            # Convert list of Series to a DataFrame for easier aggregation
            runs_df = pd.DataFrame(sensor_data[sensor_count])
            
            row = {"sensor_count": sensor_count}
            for metric in metrics:
                row[f"{metric}_mean"] = runs_df[metric].mean()
                row[f"{metric}_std"] = runs_df[metric].std()
            
            summary_results.append(row)
            
        # Create summary DataFrame and save to CSV
        summary_df = pd.DataFrame(summary_results)
        output_file = os.path.join(output_dir, f"summary_{b_type}.csv")
        summary_df.to_csv(output_file, index=False)
        print(f"Saved summary to {output_file}")

if __name__ == "__main__":
    process_benchmarks()
