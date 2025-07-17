
# Práctica 2: Sistema de búsqueda de datos de spotify usando SOcket

Este proyecto implementa un sistema de búsqueda de alto rendimiento en C para consultar un dataset masivo de canciones de Spotify (`25,681,792 filas`). Este proyecto está diseñado con una **arquitectura cliente-servidor** y utiliza técnicas de indexación y comunicación por **sockets** para manejar grandes volúmenes de datos (7 GB) de manera eficiente y rápida.

## Autores

*   **Nombres:** José Leonardo Pinilla, Frank Olmos, Juan David Ladino
*   **Curso:** Sistemas Operativos
*   **Universidad:** Universidad Nacional de Colombia

## Características Principales

*   **Arquitectura Cliente-Servidor:** El sistema se divide en componentes independientes: un indexador (`indexer`), un servidor de búsqueda (`searcher_s`) y un cliente con interfaz gráfica (`ui_client`).
*   **Comunicación por Sockets:** La comunicación entre el cliente y el servidor de búsqueda se realiza de forma robusta mediante **Sockets (TCP/IP)**, permitiendo una arquitectura desacoplada y escalable.
*   **Indexación Eficiente:** Se implementa un proceso de indexación que lee el dataset de 7 GB una sola vez y genera un **índice binario** optimizado para búsquedas rápidas.
*   **Tabla Hash:** El núcleo de la búsqueda se basa en una **tabla hash** con manejo de colisiones (encadenamiento en disco) para un acceso a los datos en tiempo casi constante.
*   **Bajo Consumo de Memoria:** El servidor de búsqueda (`searcher_s`) fue diseñado para cumplir un estricto límite de **<10 MB de RAM**, manteniendo el dataset y la mayor parte del índice en disco.
*   **Interfaz Gráfica (GUI):** Se desarrolló una interfaz de usuario amigable con la librería **GTK3**, permitiendo una interacción intuitiva.
*   **Lógica de Búsqueda Avanzada:**
    *   Búsqueda por criterios obligatorios (`Álbum` y `Artista`).
    *   Filtro opcional con búsqueda parcial e insensible a mayúsculas/minúsculas para el `Nombre de la Canción`.
    *   Salida de resultados formateada para una fácil lectura.

## Requisitos Previos

Para compilar y ejecutar este proyecto, necesitarás:

*   El compilador `gcc` y las herramientas de construcción `make`.
*   La librería de desarrollo de GTK3 (`libgtk-3-dev` en Debian/Ubuntu).

## Dataset

El proyecto utiliza el dataset "Spotify Tracks DB" disponible en Hugging Face.
*   **Enlace:** [https://huggingface.co/datasets/bigdata-pw/Spotify](https://huggingface.co/datasets/bigdata-pw/Spotify)

El archivo `spotify_data.csv` que usa el proyecto fue generado a partir de los archivos `.parquet` alojados en HuggingFace (se usó la biblioteca de Python `datasets` para unificarlos).

## Compilación

El proyecto incluye un `Makefile` que automatiza todo el proceso de compilación. Simplemente ejecuta:

```bash
make
```

Esto generará los tres ejecutables necesarios: `indexer`, `searcher_s` y `ui_client`.

## Modo de Uso

Para que todo funcione, sigue estos pasos en orden:

1.  **Generar el Índice (una sola vez):**
    Este paso lee el archivo `spotify_data.csv` y crea `spotify.index`. **Puede tardar varios segundos.**
    ```bash
    make index
    ```

2.  **Iniciar el Servidor de Búsqueda:**
    Abre una terminal y ejecuta el siguiente comando. **Esta terminal debe permanecer abierta** mientras usas la aplicación, ya que es el proceso que escucha las peticiones de búsqueda.
    ```bash
    make run-searcher
    ```

3.  **Iniciar el Cliente Gráfico:**
    Abre una **segunda terminal** y ejecuta:
    ```bash
    make run-ui UI_ARGS="IP PORT"
    ```

4.  **Realizar Búsquedas:** Utiliza la ventana que aparecerá para introducir los criterios y buscar.

## Limpieza

Para eliminar todos los archivos generados (ejecutables y el archivo de índice), ejecuta:

```bash
make clean
```
