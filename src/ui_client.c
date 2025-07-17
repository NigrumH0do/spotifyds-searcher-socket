/*
 * ui_client.c: Proceso CLIENTE con interfaz GTK.
 * Se conecta a un servidor de búsqueda de Spotify a través de un socket TCP/IP.
 * Envía consultas de búsqueda y muestra los resultados recibidos.
 * La IP y el puerto del servidor se pasan como argumentos de línea de comandos.
 */
#include <gtk/gtk.h>
#include <string.h>
#include <unistd.h>
#include <sys/socket.h>
#include <netinet/in.h>
#include <arpa/inet.h>

#define MAX_RESULTS_BUFFER 65536

// Estructura para pasar datos a los callbacks de GTK
typedef struct {
    GtkWidget *album_entrada;
    GtkWidget *artista_entrada;
    GtkWidget *cancion_entrada;
    GtkTextBuffer *buffer_resultado;
    char *server_ip; // IP del servidor
    int server_port; // Puerto del servidor
} AppWidgets;

// Función que se ejecuta al presionar el botón de búsqueda
static void buscar_accion(GtkButton *button, gpointer user_data) {
    AppWidgets *widgets = (AppWidgets *)user_data;

    const char *album_q = gtk_entry_get_text(GTK_ENTRY(widgets->album_entrada));
    const char *artista_q = gtk_entry_get_text(GTK_ENTRY(widgets->artista_entrada));
    const char *cancion_q = gtk_entry_get_text(GTK_ENTRY(widgets->cancion_entrada));

    if (strlen(album_q) == 0 || strlen(artista_q) == 0) {
        gtk_text_buffer_set_text(widgets->buffer_resultado, "Error: Los campos de Álbum y Artista son obligatorios.", -1);
        return;
    }

    char query_string[1024];
    snprintf(query_string, sizeof(query_string), "%s|%s|%s", album_q, artista_q, cancion_q);

    // --- Lógica de Sockets (Cliente) ---
    int sock = 0;
    struct sockaddr_in serv_addr;
    char buffer_resultado[MAX_RESULTS_BUFFER] = {0};

    // Crear socket
    if ((sock = socket(AF_INET, SOCK_STREAM, 0)) < 0) {
        gtk_text_buffer_set_text(widgets->buffer_resultado, "Error: Falló la creación del socket.", -1);
        return;
    }

    serv_addr.sin_family = AF_INET;
    serv_addr.sin_port = htons(widgets->server_port);

    // Convertir dirección IP de texto a binario
    if (inet_pton(AF_INET, widgets->server_ip, &serv_addr.sin_addr) <= 0) {
        gtk_text_buffer_set_text(widgets->buffer_resultado, "Error: Dirección IP inválida o no soportada.", -1);
        close(sock);
        return;
    }

    // Conectar al servidor
    if (connect(sock, (struct sockaddr *)&serv_addr, sizeof(serv_addr)) < 0) {
        char error_msg[256];
        snprintf(error_msg, sizeof(error_msg), "Error: No se pudo conectar al servidor en %s:%d.", widgets->server_ip, widgets->server_port);
        gtk_text_buffer_set_text(widgets->buffer_resultado, error_msg, -1);
        close(sock);
        return;
    }

    // Actualizar UI para mostrar que se está buscando
    gtk_text_buffer_set_text(widgets->buffer_resultado, "Buscando...", -1);
    while (gtk_events_pending()) { gtk_main_iteration(); }

    // Enviar la consulta al servidor
    send(sock, query_string, strlen(query_string), 0);

    // Leer la respuesta del servidor
    ssize_t bytes_read = read(sock, buffer_resultado, sizeof(buffer_resultado) - 1);

    if (bytes_read > 0) {
        buffer_resultado[bytes_read] = '\0';
        gtk_text_buffer_set_text(widgets->buffer_resultado, buffer_resultado, -1);
    } else {
        gtk_text_buffer_set_text(widgets->buffer_resultado, "No se recibió respuesta del servidor o la conexión se cerró.", -1);
    }

    // Cerrar el socket
    close(sock);
}

int main(int argc, char *argv[]) {
    // Validar argumentos de línea de comandos
    if (argc != 3) {
        fprintf(stderr, "Uso: %s <IP_DEL_SERVIDOR> <PUERTO>\n", argv[0]);
        return 1;
    }

    char *server_ip = argv[1];
    int server_port = atoi(argv[2]);

    gtk_init(&argc, &argv);

    GtkWidget *window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Cliente Buscador de Spotify");
    gtk_window_set_default_size(GTK_WINDOW(window), 700, 500);
    gtk_container_set_border_width(GTK_CONTAINER(window), 15);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    GtkWidget *grid = gtk_grid_new();
    gtk_grid_set_row_spacing(GTK_GRID(grid), 10);
    gtk_grid_set_column_spacing(GTK_GRID(grid), 10);
    gtk_container_add(GTK_CONTAINER(window), grid);

    AppWidgets *widgets = g_slice_new(AppWidgets);
    widgets->server_ip = server_ip;
    widgets->server_port = server_port;

    // Criterios de búsqueda
    widgets->album_entrada = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->album_entrada), "Criterio 1: Obligatorio");
    widgets->artista_entrada = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->artista_entrada), "Criterio 2: Obligatorio");
    widgets->cancion_entrada = gtk_entry_new();
    gtk_entry_set_placeholder_text(GTK_ENTRY(widgets->cancion_entrada), "Criterio 3: Opcional");

    // Área de resultados con Scroll
    GtkWidget *scrolled_window = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrolled_window), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_widget_set_hexpand(scrolled_window, TRUE);
    gtk_widget_set_vexpand(scrolled_window, TRUE);
    GtkWidget *vista_salir = gtk_text_view_new();
    gtk_text_view_set_editable(GTK_TEXT_VIEW(vista_salir), FALSE);
    widgets->buffer_resultado = gtk_text_view_get_buffer(GTK_TEXT_VIEW(vista_salir));
    gtk_container_add(GTK_CONTAINER(scrolled_window), vista_salir);

    // Botones
    GtkWidget *boton_buscar = gtk_button_new_with_label("Realizar Búsqueda Remota");
    GtkWidget *boton_salir = gtk_button_new_with_label("Salir");

    // Añadir widgets a la grilla
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Álbum:"), 0, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widgets->album_entrada, 1, 0, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Artista:"), 0, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widgets->artista_entrada, 1, 1, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Canción (Opcional):"), 0, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), widgets->cancion_entrada, 1, 2, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), boton_buscar, 0, 3, 2, 1);
    gtk_grid_attach(GTK_GRID(grid), boton_salir, 2, 3, 1, 1);
    gtk_grid_attach(GTK_GRID(grid), gtk_label_new("Resultados:"), 0, 4, 3, 1);
    gtk_grid_attach(GTK_GRID(grid), scrolled_window, 0, 5, 3, 1);

    g_signal_connect(boton_buscar, "clicked", G_CALLBACK(buscar_accion), widgets);
    g_signal_connect(boton_salir, "clicked", G_CALLBACK(gtk_main_quit), NULL);

    gtk_widget_show_all(window);
    gtk_main();

    g_slice_free(AppWidgets, widgets);
    return 0;
}
