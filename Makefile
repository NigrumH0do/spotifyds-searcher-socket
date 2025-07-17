# ---------------------------------
# Makefile para el Proyecto de Búsqueda Spotify
# Compila el indexador, el servidor de búsqueda y la interfaz de cliente.
# ---------------------------------

# --- Variables de Compilación ---

# Compilador de C a utilizar
CC = gcc

CFLAGS = -g -Wall

# --- Variables para Librerías Externas (GTK3) ---
GTK_LIBS = $(shell pkg-config --cflags --libs gtk+-3.0)


# --- Nombres de Archivos Fuente y Ejecutables ---

INDEXER_SRC = src/indexer.c
SEARCHER_SRC = src/searcher_s.c
UI_SRC = src/ui_client.c

INDEXER_EXEC = indexer
SEARCHER_EXEC = searcher_s
UI_EXEC = ui_client

TARGETS = $(INDEXER_EXEC) $(SEARCHER_EXEC) $(UI_EXEC)


# --- Reglas de Compilación ---

# Regla por defecto: se ejecuta al escribir 'make'. Compila todos los programas.
all: $(TARGETS)

# Regla para compilar el indexador
$(INDEXER_EXEC): $(INDEXER_SRC)
	$(CC) $(CFLAGS) -o $@ $<

# Regla para compilar el servidor de búsqueda
$(SEARCHER_EXEC): $(SEARCHER_SRC)
	$(CC) $(CFLAGS) -o $@ $<

# Regla para compilar la interfaz de cliente con GTK
$(UI_EXEC): $(UI_SRC)
	$(CC) $(CFLAGS) -o $@ $< $(GTK_LIBS)


# --- Reglas de Utilidad y Ejecución ---

# Declara las reglas que no generan archivos con su mismo nombre
.PHONY: all clean index run-searcher run-ui

# Regla para ejecutar el indexador. Primero se asegura de que esté compilado.
index: $(INDEXER_EXEC)
	@echo "--- Ejecutando el Indexador para crear spotify.index ---"
	./$(INDEXER_EXEC)

# Regla para ejecutar el servidor de búsqueda.
run-searcher: $(SEARCHER_EXEC)
	@echo "--- Iniciando el servidor de búsqueda ---"
	./$(SEARCHER_EXEC)

# Regla para ejecutar la interfaz gráfica de cliente.
run-ui: $(UI_EXEC)
	@echo "--- Iniciando la Interfaz Gráfica (Cliente) ---"
	./$(UI_EXEC) $(UI_ARGS)

# Regla para limpiar el directorio de ejecutables y el archivo de índice
clean:
	@echo "--- Limpiando archivos compilados y el índice generado ---"
	rm -f $(TARGETS) spotify.index
