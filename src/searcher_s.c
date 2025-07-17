/*
* searcher_server.c: Proceso SERVIDOR de búsqueda de Spotify.
* Escucha en un puerto TCP, acepta conexiones de clientes, recibe consultas,
* busca en el índice local y devuelve los resultados a través del socket.
* Utiliza fork() para manejar múltiples clientes de forma concurrente.
*/
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <signal.h>
#include <sys/wait.h>
#include <errno.h> 
#include <arpa/inet.h> // Para inet_ntop

#define PORT 8080 // Puerto en el que escucha el servidor
#define HASH_TABLE_SIZE 500000
#define MAX_LINE_LENGTH 8192
#define MAX_KEY_LENGTH 512
#define MAX_RESULTS_BUFFER 65536

typedef struct Nodo {
    long csv_puntero;
    long siguiente_nodo_puntero;
} Nodo;

// Globales para los archivos y la tabla hash
long *hash_table;
FILE *csv_file;

// --- Declaraciones de Funciones ---
char *buscar_campo(const char *line, int campoABuscar);
unsigned long hash_function(const char *str);
char *extraer_artista(const char *json_string);
void formato_resultado(char *dest, size_t dest_size, const char *csv_line);
void handle_client(int client_socket);
void sigchld_handler(int s);

int main(int argc, char *argv[]) {
    // --- Carga de datos (índice y CSV) ---
    FILE *index_file = fopen("spotify.index", "rb");
    if (!index_file) {
        perror("FATAL: No se pudo abrir 'spotify.index'. Ejecute el indexador primero.");
        return 1;
    }
    hash_table = (long *)malloc(sizeof(long) * HASH_TABLE_SIZE);
    if (!hash_table) {
        perror("FATAL: No se pudo alocar memoria para la tabla hash");
        fclose(index_file);
        return 1;
    }
    fread(hash_table, sizeof(long), HASH_TABLE_SIZE, index_file);
    fclose(index_file);

    csv_file = fopen("spotify_data.csv", "r");
    if (!csv_file) {
        free(hash_table);
        perror("FATAL: No se pudo abrir 'spotify_data.csv'");
        return 1;
    }

    // --- Configuración del Servidor Socket ---
    int server_fd, new_socket;
    struct sockaddr_in address;
    int opt = 1;
    int addrlen = sizeof(address);

    // Creación del descriptor de archivo del socket
    if ((server_fd = socket(AF_INET, SOCK_STREAM, 0)) == 0) {
        perror("socket failed");
        exit(EXIT_FAILURE);
    }

    // Adjuntar el socket al puerto 8080
    if (setsockopt(server_fd, SOL_SOCKET, SO_REUSEADDR | SO_REUSEPORT, &opt, sizeof(opt))) {
        perror("setsockopt");
        exit(EXIT_FAILURE);
    }
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY; // Escuchar en todas las interfaces de red
    address.sin_port = htons(PORT);

    // Vincular el socket al puerto
    if (bind(server_fd, (struct sockaddr *)&address, sizeof(address)) < 0) {
        perror("bind fallo");
        exit(EXIT_FAILURE);
    }

    // Poner el servidor en modo de escucha
    if (listen(server_fd, 10) < 0) { // Backlog de 10 conexiones
        perror("listen");
        exit(EXIT_FAILURE);
    }

    //Manejador de señales para procesos hijos (zombies)
    struct sigaction sa;
    sa.sa_handler = sigchld_handler; // Usar nuestra función para limpiar zombies
    sigemptyset(&sa.sa_mask);
    sa.sa_flags = SA_RESTART; // Evita tener que manejar interrupciones en llamadas bloqueantes.
    if (sigaction(SIGCHLD, &sa, NULL) == -1) {
        perror("sigaction");
        exit(1);
    }

    printf("Servidor de búsqueda escuchando en el puerto %d\n", PORT);

    // aceptar conexiones 
    while (1) {
        if ((new_socket = accept(server_fd, (struct sockaddr *)&address, (socklen_t*)&addrlen)) < 0) {
            perror("accept");
            continue; // Continuar esperando si accept falla
        }

        // Crear un proceso hijo para manejar al cliente
        if (!fork()) { //proceso hijo
            close(server_fd); // Cierra socket de servidor del padre.
            handle_client(new_socket);
            close(new_socket);
            exit(0);
        }
        // El proceso padre sigue aquí
        close(new_socket); // Cierra socket del cliente en el padre.
    }

    // --- Limpieza
    free(hash_table);
    fclose(csv_file);
    return 0;
}

// Manejador para limpiar procesos hijos terminados
void sigchld_handler(int s) {
    int saved_errno = errno;
    while(waitpid(-1, NULL, WNOHANG) > 0);
    errno = saved_errno;
}

// Función que maneja la lógica de una conexión de cliente
void handle_client(int client_socket) {
    struct sockaddr_in client_addr;
    socklen_t addr_len = sizeof(client_addr);
    char client_ip[INET_ADDRSTRLEN];

    //Obtener la información de la dirección del cliente desde el socket
    if (getpeername(client_socket, (struct sockaddr*)&client_addr, &addr_len) == 0) {
        //Convertir la IP de formato binario a texto y guardarla en client_ip
        inet_ntop(AF_INET, &client_addr.sin_addr, client_ip, sizeof(client_ip));
    } else {
        perror("getpeername failed");
    }
    char query_buffer[MAX_KEY_LENGTH * 2] = {0};
    ssize_t bytes_read = read(client_socket, query_buffer, sizeof(query_buffer) - 1);

    if (bytes_read > 0) {
        query_buffer[bytes_read] = '\0';
        char query_copy[sizeof(query_buffer)];
        strcpy(query_copy, query_buffer);

        char *album_q = strtok(query_copy, "|");
        char *artista_q = strtok(NULL, "|");
        char *cancion_q = strtok(NULL, "|");

        if (!album_q || !artista_q) {
            char *error_msg = "Error: Consulta inválida.";
            send(client_socket, error_msg, strlen(error_msg), 0);
            return;
        }
        printf("IP %s : Album '%s' | Artista '%s'\n", client_ip, album_q, artista_q);

        char composite_key[MAX_KEY_LENGTH];
        snprintf(composite_key, sizeof(composite_key), "%s|%s", album_q, artista_q);
        
        char final_result[MAX_RESULTS_BUFFER] = {0};
        int encontrados_cuenta = 0;
        unsigned long index = hash_function(composite_key);
        long campo_nodo_index = hash_table[index];
        
        FILE *idx_f = fopen("spotify.index", "rb");
        char *line_buffer = malloc(MAX_LINE_LENGTH);

        if (idx_f && line_buffer) {
            while (campo_nodo_index != -1) {
                fseek(idx_f, campo_nodo_index, SEEK_SET);
                Nodo current_node;
                fread(&current_node, sizeof(Nodo), 1, idx_f);
                fseek(csv_file, current_node.csv_puntero, SEEK_SET);
                fgets(line_buffer, MAX_LINE_LENGTH, csv_file);

                char *album_from_csv = buscar_campo(line_buffer, 1);
                char *artista_json_from_csv = buscar_campo(line_buffer, 4);
                char *artist_from_csv = artista_json_from_csv ? extraer_artista(artista_json_from_csv) : NULL;

                if (album_from_csv && artist_from_csv &&
                    strcmp(album_from_csv, album_q) == 0 &&
                    strcmp(artist_from_csv, artista_q) == 0) {
                    
                    int match = 0;
                    if (cancion_q == NULL || strlen(cancion_q) == 0) {
                        match = 1;
                    } else {
                        char *cancion_from_csv = buscar_campo(line_buffer, 8);
                        if (cancion_from_csv && strcasestr(cancion_from_csv, cancion_q) != NULL) {
                            match = 1;
                        }
                        if (cancion_from_csv) free(cancion_from_csv);
                    }

                    if (match) {
                        char formatted_line[MAX_LINE_LENGTH];
                        formato_resultado(formatted_line, sizeof(formatted_line), line_buffer);
                        if (strlen(final_result) + strlen(formatted_line) < MAX_RESULTS_BUFFER) {
                            strcat(final_result, formatted_line);
                            encontrados_cuenta++;
                        }
                    }
                }
                if (album_from_csv) free(album_from_csv);
                if (artista_json_from_csv) free(artista_json_from_csv);
                if (artist_from_csv) free(artist_from_csv);
                
                campo_nodo_index = current_node.siguiente_nodo_puntero;
            }
            fclose(idx_f);
            free(line_buffer);
        }

        if (encontrados_cuenta == 0) {
            strcpy(final_result, "No se encontraron resultados para la búsqueda.");
        }
        
        send(client_socket, final_result, strlen(final_result), 0);
    }
}


void formato_resultado(char *dest, size_t dest_size, const char *csv_line) {
    char *album = buscar_campo(csv_line, 1);
    char *artista_json = buscar_campo(csv_line, 4);
    char *artist = artista_json ? extraer_artista(artista_json) : NULL;
    char *cancion = buscar_campo(csv_line, 8);
    char *duracion_str = buscar_campo(csv_line, 6);
    char *popularidad_str = buscar_campo(csv_line, 10);

    char duration_formatted[32] = "N/A";
    if (duracion_str) {
        long duration_ms = atol(duracion_str);
        int minutes = duration_ms / 60000;
        int seconds = (duration_ms % 60000) / 1000;
        snprintf(duration_formatted, sizeof(duration_formatted), "%d min %d seg", minutes, seconds);
    }

    snprintf(dest, dest_size,
             "Álbum: %s\n"
             "Artista: %s\n"
             "Canción: %s\n"
             "Duración: %s\n"
             "Popularidad: %s\n"
             "--------------------------------------------------\n",
             album ? album : "N/A",
             artist ? artist : "N/A",
             cancion ? cancion : "N/A",
             duration_formatted,
             popularidad_str ? popularidad_str : "N/A");

    if (album) free(album);
    if (artista_json) free(artista_json);
    if (artist) free(artist);
    if (cancion) free(cancion);
    if (duracion_str) free(duracion_str);
    if (popularidad_str) free(popularidad_str);
}

unsigned long hash_function(const char *str) {
    unsigned long hash = 5381;
    int c;
    while ((c = *str++)) {
        hash = ((hash << 5) + hash) + c;
    }
    return hash % HASH_TABLE_SIZE;
}

char *buscar_campo(const char *line, int campoABuscar) {
    char *buffer = malloc(MAX_LINE_LENGTH);
    if (!buffer) return NULL;
    int campoActual = 1, in_quotes = 0, buffer_pos = 0;
    const char *line_ptr = line;
    while (*line_ptr) {
        if (*line_ptr == '"' && *(line_ptr + 1) != '"') {
            in_quotes = !in_quotes;
        } else if (*line_ptr == ',' && !in_quotes) {
            if (campoActual == campoABuscar) {
                buffer[buffer_pos] = '\0';
                return buffer;
            }
            campoActual++;
            buffer_pos = 0;
        } else {
            if (campoActual == campoABuscar) {
                if (buffer_pos < MAX_LINE_LENGTH - 1) {
                    buffer[buffer_pos++] = *line_ptr;
                }
            }
        }
        line_ptr++;
    }
    if (campoActual == campoABuscar) {
        if (buffer_pos > 0 && buffer[buffer_pos - 1] == '\n') buffer_pos--;
        buffer[buffer_pos] = '\0';
        return buffer;
    }
    free(buffer);
    return NULL;
}

char *extraer_artista(const char *json_string) {
    const char *key = "'artist_name': '";
    const char *comienzo = strstr(json_string, key);
    if (!comienzo) return strdup(json_string);
    
    comienzo += strlen(key);
    const char *final = strchr(comienzo, '\'');
    if (!final) return strdup(json_string);
    
    size_t length = final - comienzo;
    char *nombre = malloc(length + 1);
    if (nombre) {
        strncpy(nombre, comienzo, length);
        nombre[length] = '\0';
    }
    return nombre;
}
