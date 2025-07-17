/*
Proceso de Indexación de Datos de Spotify
* Este proceso lee un archivo CSV de Spotify, extrae información relevante y crea un índice
* que se almacena en un archivo binario. Utiliza una tabla hash para optimizar
* la búsqueda de álbumes y artistas.
* El índice se compone de nodos que contienen punteros a las líneas del CSV.
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#define HASH_TABLE_SIZE 500000
#define MAX_LINE_LENGTH 4096
#define MAX_KEY_LENGTH 512


typedef struct Nodo
{
    long csv_puntero; // Puntero al inicio de la línea en el CSV
    long siguiente_nodo_puntero; // Puntero al siguiente nodo en la lista enlazada
} Nodo;

//Declaración de la tabla hash
long hash_table[HASH_TABLE_SIZE];

char *get_campo(const char *line, int field_index);
unsigned long hash_function(const char *str);

/* * Función para extraer el nombre del artista de un JSON.
 * Esta función unicamente extrae la primera coincidencia del campo 'artist_name'.
 * con un formato de Ejemplo: {
    "artist_gid": "f81b117976f14636ba5caea759b82e67",
    "artist_name": "NormanBate$",
    "role": "ARTIST_ROLE_MAIN_ARTIST"
  }
]
*/ 
char *extraer_artista(const char *json_string)
{
    const char *key = "'artist_name': '"; 
    const char *start = strstr(json_string, key);
    if (!start)
    {
        //Hace malloc interno y copia el json en un nuevo string.
        //Esto es importante para evitar problemas de memoria.
        //Si devuelve el mismo puntero al final del main habrá un
        //problema de memoria.
        return strdup(json_string);
    }
    start += strlen(key);

    const char *end = strchr(start, '\''); 
    if (!end)
    {   
        //Hace malloc interno y copia el json en un nuevo string.
        //Esto es importante para evitar problemas de memoria.
        //Si devuelve el mismo puntero al final del main habrá un
        //problema de memoria.
        return strdup(json_string);
    }

    size_t length = end - start;
    char *name = malloc(length + 1);
    if (name)
    {
        strncpy(name, start, length);
        name[length] = '\0';
    }
    return name;
}

int main(int argc, char *argv[])
{
    
    FILE *csv_file = fopen("spotify_data.csv", "r");
    if (!csv_file)
    {
        perror("Error al abrir spotify_data.csv");
        return 1;
    }
    FILE *index_file = fopen("spotify.index", "wb");
    if (!index_file)
    {
        perror("Error al crear spotify.index");
        fclose(csv_file);
        return 1;
    }
    for (int i = 0; i < HASH_TABLE_SIZE; i++)
    {
        hash_table[i] = -1;
    }
    char *line = malloc(MAX_LINE_LENGTH);
    // --- 4.2: Preparación del Archivo de Índice ---
    // Mueve el cursor de escritura del archivo de índice hacia adelante.
    // Deja un espacio en blanco al principio del tamaño de la tabla hash.
    fseek(index_file, sizeof(long) * HASH_TABLE_SIZE, SEEK_SET);
    // Lee la primera línea del CSV (la cabecera) y la descarta.
    fgets(line, MAX_LINE_LENGTH, csv_file);
    // Guarda las posiciones iniciales en bytes de ambos archivos.
    long puntero_actual_csv = ftell(csv_file);
    long puntero_actual_index = ftell(index_file);
    printf("Generando index...\n");
    int count = 0;
    while (fgets(line, MAX_LINE_LENGTH, csv_file) != NULL)
    {
        char *album_nombre = get_campo(line, 1);
        char *artista_json = get_campo(line, 4);
        if (album_nombre != NULL && artista_json != NULL)
        {
            char *artist_name = extraer_artista(artista_json);
            if (artist_name)
            {
                char composite_key[MAX_KEY_LENGTH];
                snprintf(composite_key, sizeof(composite_key), "%s|%s", album_nombre, artist_name);
                if (strlen(composite_key) > 1)
                {
                    unsigned long index = hash_function(composite_key);
                    Nodo new_node = {puntero_actual_csv, hash_table[index]};
                    fwrite(&new_node, sizeof(Nodo), 1, index_file);
                    hash_table[index] = puntero_actual_index;
                    puntero_actual_index = ftell(index_file);
                }
                free(artist_name);
            }
        }
        if (album_nombre)
            free(album_nombre);
        if (artista_json)
            free(artista_json);
        puntero_actual_csv = ftell(csv_file);
        if (++count % 100000 == 0)
        {
            printf("Procesadas %d líneas...\n", count);
            fflush(stdout);
        }
    }
    // Volver a donde estaba el espacio el blanco.
    fseek(index_file, 0, SEEK_SET);
    fwrite(hash_table, sizeof(long), HASH_TABLE_SIZE, index_file);
    printf("¡Índice final creado exitosamente en 'spotify.index'!\n");
    free(line);
    fclose(csv_file);
    fclose(index_file);
    return 0;
}

// Las implementaciones de get_campo y hash_function no cambian
unsigned long hash_function(const char *str)
{
    unsigned long hash = 5381;
    int c;
    while ((c = *str++))
    {   
        //Desplazamiento de bits por izquierda 
        hash = ((hash << 5) + hash) + c; //Lo mismo que hash * 33 + c
    }
    return hash % HASH_TABLE_SIZE;
}
char *get_campo(const char *line, int field_index)
{
    char *buffer = malloc(MAX_LINE_LENGTH);
    if (!buffer)
        return NULL;
    // Columna actual, bandera y posición del buffer
    int get_campo = 1, in_quotes = 0, buffer_pos = 0;
    const char *line_ptr = line;
    while (*line_ptr)
    {
        if (*line_ptr == '"' && *(line_ptr + 1) != '"')
        {
            in_quotes = !in_quotes;
        }
        else if (*line_ptr == ',' && !in_quotes)
        {
            if (get_campo == field_index)
            {
                buffer[buffer_pos] = '\0';
                return buffer;
            }
            get_campo++;
            buffer_pos = 0;
        }
        else
        {
            if (get_campo == field_index)
            {
                if (buffer_pos < MAX_LINE_LENGTH - 1)
                    buffer[buffer_pos++] = *line_ptr;
            }
        }
        line_ptr++;
    }
    if (get_campo == field_index)
    {
        if (buffer_pos > 0 && buffer[buffer_pos - 1] == '\n')
            buffer_pos--;
        buffer[buffer_pos] = '\0';
        return buffer;
    }
    free(buffer);
    return NULL;
}