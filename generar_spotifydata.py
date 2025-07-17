from datasets import load_dataset
import pandas as pd

# Hugging Face
repo_id = "bigdata-pw/Spotify"

try:
    spotify_dataset = load_dataset(repo_id)

    print("Convirtiendo los datos a un DataFrame de pandas...")
    spotify_df = spotify_dataset['train'].to_pandas()

    print(f"El DataFrame unificado tiene {spotify_df.shape[0]} filas y {spotify_df.shape[1]} columnas.")

    # Nombre del archivo de salida
    output_filename = "spotify_data.csv"

    print(f"\nGuardando los datos unificados en el archivo: '{output_filename}'...")
    spotify_df.to_csv(output_filename, index=False)

    print(f"\n¡Proceso completado! Todos los archivos han sido unificados en '{output_filename}'")

except Exception as e:
    print(f"Ocurrió un error: {e}")