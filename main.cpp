#include <gtk/gtk.h>
#include <gdk/gdk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>
#include <algorithm>
#include <vector>
#include <iostream>
#include <thread>
#include <string>


// Global variables
GtkWidget *window;
GtkWidget *image_viewer; // Image viewing widget
GtkWidget *thumbnails_box; // Box component for displaying thumbnails
GtkWidget *thumbnails_scroller; // Scrolled window for the thumbnails
GdkPixbuf *current_pixbuf = nullptr; // Currently displayed image
double zoom_factor = 1.0; // Zoom level (1.0 = 100%)
gchar *current_filename = nullptr; // Currently open filename
int selected_thumbnail = -1; // Index of the selected thumbnail
int thumbnail_count = 5; // Number of thumbnails to display
std::vector<std::string> thumbnail_files; // List of thumbnail file names

// Function declarations
static void update_image_viewer();
static void zoom_in_menu_item_clicked(GtkWidget *widget, gpointer data);
static void zoom_out_menu_item_clicked(GtkWidget *widget, gpointer data);
static void open_menu_item_clicked(GtkWidget *widget, gpointer data);
static void save_menu_item_clicked(GtkWidget *widget, gpointer data);
static void reset_zoom_menu_item_clicked(GtkWidget *widget, gpointer data);
static void create_thumbnails(const gchar *directory);
static void thumbnail_clicked(GtkWidget *widget, gpointer data);
static GtkWidget* create_menu_item(const gchar *label, const gchar *icon_name, GCallback callback, gpointer data, const gchar *accelerator, const gchar *tooltip);
static GdkPixbuf* create_scaled_pixbuf(GdkPixbuf *src, int width, int height);
static void update_gui();
static void select_thumbnail(int index);
static void next_thumbnail();
static void previous_thumbnail();
static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data);
static void load_thumbnails_async(const gchar *directory);

// Function to update the image viewer
static void update_image_viewer()
{
    if (current_pixbuf != nullptr)
    {
        int width = gdk_pixbuf_get_width(current_pixbuf);
        int height = gdk_pixbuf_get_height(current_pixbuf);

        GdkPixbuf *scaled_pixbuf = gdk_pixbuf_scale_simple(current_pixbuf, width * zoom_factor, height * zoom_factor, GDK_INTERP_BILINEAR);
        gtk_image_set_from_pixbuf(GTK_IMAGE(image_viewer), scaled_pixbuf);
        g_object_unref(scaled_pixbuf);

        GtkAdjustment *adj_h = gtk_scrolled_window_get_hadjustment(GTK_SCROLLED_WINDOW(thumbnails_scroller));
        GtkAdjustment *adj_v = gtk_scrolled_window_get_vadjustment(GTK_SCROLLED_WINDOW(thumbnails_scroller));

        gtk_adjustment_set_value(adj_h, 0);
        gtk_adjustment_set_value(adj_v, 0);
    }
    else
    {
        gtk_image_clear(GTK_IMAGE(image_viewer));
    }
}

// Function to handle zoom in menu item click
static void zoom_in_menu_item_clicked(GtkWidget *widget, gpointer data)
{
    zoom_factor *= 1.1;
    update_image_viewer();
}

// Function to handle zoom out menu item click
static void zoom_out_menu_item_clicked(GtkWidget *widget, gpointer data)
{
    zoom_factor /= 1.1;
    update_image_viewer();
}

// Function to handle open menu item click
static void open_menu_item_clicked(GtkWidget *widget, gpointer data)
{
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Open File", GTK_WINDOW(data), GTK_FILE_CHOOSER_ACTION_OPEN,
        "_Cancel", GTK_RESPONSE_CANCEL,
        "_Open", GTK_RESPONSE_ACCEPT,
        NULL);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Images");
    gtk_file_filter_add_mime_type(filter, "image/jpeg");
    gtk_file_filter_add_mime_type(filter, "image/png");
    gtk_file_filter_add_pattern(filter, "*.jpg");
    gtk_file_filter_add_pattern(filter, "*.jpeg");
    gtk_file_filter_add_pattern(filter, "*.png");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_ACCEPT)
    {
        gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

        GError *error = nullptr;
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(filename, &error);
        if (pixbuf == nullptr)
        {
            GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                                            GTK_DIALOG_DESTROY_WITH_PARENT,
                                                            GTK_MESSAGE_ERROR,
                                                            GTK_BUTTONS_CLOSE,
                                                            "Error opening image: %s",
                                                            error->message);
            gtk_dialog_run(GTK_DIALOG(error_dialog));
            gtk_widget_destroy(error_dialog);
            g_error_free(error);
        }
        else
        {
            if (current_pixbuf != nullptr)
                g_object_unref(current_pixbuf);
            current_pixbuf = pixbuf;
            zoom_factor = 1.0;
            update_image_viewer();
            g_clear_pointer(&current_filename, g_free);
            current_filename = g_strdup(filename);
            load_thumbnails_async(g_path_get_dirname(filename));
        }

        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

// Function to handle save menu item click
static void save_menu_item_clicked(GtkWidget *widget, gpointer data)
{
    if (current_pixbuf != nullptr)
    {
        GtkWidget *dialog = gtk_file_chooser_dialog_new(
            "Save Image", GTK_WINDOW(data), GTK_FILE_CHOOSER_ACTION_SAVE,
            "_Cancel", GTK_RESPONSE_CANCEL,
            "_Save", GTK_RESPONSE_ACCEPT,
            NULL);

        GtkFileFilter *filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, "JPEG");
        gtk_file_filter_add_mime_type(filter, "image/jpeg");
        gtk_file_filter_add_pattern(filter, "*.jpg");
        gtk_file_filter_add_pattern(filter, "*.jpeg");
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

        filter = gtk_file_filter_new();
        gtk_file_filter_set_name(filter, "PNG");
        gtk_file_filter_add_mime_type(filter, "image/png");
        gtk_file_filter_add_pattern(filter, "*.png");
        gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

        gtk_file_chooser_set_current_name(GTK_FILE_CHOOSER(dialog), current_filename);

        gint response = gtk_dialog_run(GTK_DIALOG(dialog));

        if (response == GTK_RESPONSE_ACCEPT)
        {
            gchar *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

            GError *error = nullptr;
            if (!gdk_pixbuf_save(current_pixbuf, filename, "jpeg", &error))
            {
                GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                                                GTK_DIALOG_DESTROY_WITH_PARENT,
                                                                GTK_MESSAGE_ERROR,
                                                                GTK_BUTTONS_CLOSE,
                                                                "Error saving image: %s",
                                                                error->message);
                gtk_dialog_run(GTK_DIALOG(error_dialog));
                gtk_widget_destroy(error_dialog);
                g_error_free(error);
            }

            g_free(filename);
        }

        gtk_widget_destroy(dialog);
    }
}

// Function to handle reset zoom menu item click
static void reset_zoom_menu_item_clicked(GtkWidget *widget, gpointer data)
{
    zoom_factor = 1.0;
    update_image_viewer();
}

// Function to create a menu item
static GtkWidget* create_menu_item(const gchar *label, const gchar *icon_name, GCallback callback, gpointer data, const gchar *accelerator, const gchar *tooltip)
{
    GtkWidget *item = gtk_menu_item_new();

    GtkWidget *box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);

    if (icon_name != nullptr)
    {
        GtkWidget *image = gtk_image_new_from_icon_name(icon_name, GTK_ICON_SIZE_MENU);
        gtk_image_set_pixel_size(GTK_IMAGE(image), 16);
        gtk_box_pack_start(GTK_BOX(box), image, FALSE, FALSE, 0);
    }

    GtkWidget *label_widget = gtk_label_new(label);
    gtk_box_pack_start(GTK_BOX(box), label_widget, TRUE, TRUE, 0);

    gtk_container_add(GTK_CONTAINER(item), box);

    if (callback != nullptr)
        g_signal_connect(item, "activate", callback, data);

    if (accelerator != nullptr)
    {
        GtkAccelGroup *accel_group = gtk_accel_group_new();
        gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);
        gtk_widget_add_accelerator(item, "activate", accel_group, gdk_keyval_from_name(accelerator), GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    }

    if (tooltip != nullptr)
    {
        gtk_widget_set_tooltip_text(item, tooltip);
    }

    return item;
}

// Function to create a scaled GdkPixbuf
static GdkPixbuf* create_scaled_pixbuf(GdkPixbuf *src, int width, int height)
{
    int src_width = gdk_pixbuf_get_width(src);
    int src_height = gdk_pixbuf_get_height(src);

    double scale_x = (double)width / src_width;
    double scale_y = (double)height / src_height;
    double scale = std::min(scale_x, scale_y);

    int dest_width = src_width * scale;
    int dest_height = src_height * scale;

    return gdk_pixbuf_scale_simple(src, dest_width, dest_height, GDK_INTERP_BILINEAR);
}

// Function to update the GUI
static void update_gui()
{
    update_image_viewer();
    select_thumbnail(selected_thumbnail);
}


// Function to select a thumbnail
static void select_thumbnail(int index)
{
    GList *children = gtk_container_get_children(GTK_CONTAINER(thumbnails_box));
    int child_count = g_list_length(children);

    if (index >= 0 && index < child_count)
    {
        GtkWidget *child = GTK_WIDGET(g_list_nth_data(children, index));
        gtk_widget_grab_focus(child);
    }
    else
    {
        gtk_widget_grab_focus(thumbnails_scroller);
    }

    g_list_free(children);
}

// Function to handle next thumbnail action
static void next_thumbnail()
{
    GList *children = gtk_container_get_children(GTK_CONTAINER(thumbnails_box));
    int child_count = g_list_length(children);

    if (child_count > 0)
    {
        if (selected_thumbnail < child_count - 1)
            selected_thumbnail++;
        else
            selected_thumbnail = 0;

        select_thumbnail(selected_thumbnail);
    }

    g_list_free(children);
}

// Function to handle previous thumbnail action
static void previous_thumbnail()
{
    GList *children = gtk_container_get_children(GTK_CONTAINER(thumbnails_box));
    int child_count = g_list_length(children);

    if (child_count > 0)
    {
        if (selected_thumbnail > 0)
            selected_thumbnail--;
        else
            selected_thumbnail = child_count - 1;

        select_thumbnail(selected_thumbnail);
    }

    g_list_free(children);
}

// Function to handle key press events
static gboolean on_key_press(GtkWidget *widget, GdkEventKey *event, gpointer data)
{
    if (event->type == GDK_KEY_PRESS)
    {
        switch (event->keyval)
        {
            case GDK_KEY_Right:
                next_thumbnail();
                return TRUE;
            case GDK_KEY_Left:
                previous_thumbnail();
                return TRUE;
        }
    }

    return FALSE;
}

// Function to create thumbnails
static void create_thumbnails(const gchar *directory)
{
    GError *error = nullptr;
    GDir *dir = g_dir_open(directory, 0, &error);
    if (dir == nullptr)
    {
        g_error_free(error);
        return;
    }

    // Clear existing thumbnails
    GList *children = gtk_container_get_children(GTK_CONTAINER(thumbnails_box));
    g_list_free_full(children, g_object_unref);

    // Clear existing thumbnail files
    thumbnail_files.clear();

    // Create new thumbnails
    const gchar *filename;
    while ((filename = g_dir_read_name(dir)) != nullptr)
    {
        gchar *path = g_build_filename(directory, filename, nullptr);

        GError *error = nullptr;
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(path, &error);
        if (pixbuf != nullptr)
        {
            GdkPixbuf *thumbnail = create_scaled_pixbuf(pixbuf, 80, 80);
            GtkWidget *thumbnail_button = gtk_button_new();
            GtkWidget *thumbnail_image = gtk_image_new_from_pixbuf(thumbnail);
            gtk_button_set_image(GTK_BUTTON(thumbnail_button), thumbnail_image);
            gtk_box_pack_start(GTK_BOX(thumbnails_box), thumbnail_button, FALSE, FALSE, 0);
            g_object_unref(thumbnail);

            g_signal_connect(thumbnail_button, "clicked", G_CALLBACK(thumbnail_clicked), g_strdup(path));
            gtk_widget_show(thumbnail_button);

            // Add thumbnail file name to the list
            thumbnail_files.push_back(path);
        }
        else
        {
            g_error_free(error);
        }

        g_free(path);
    }

    g_dir_close(dir);
    select_thumbnail(selected_thumbnail);
}

// Function to handle thumbnail click event
static void thumbnail_clicked(GtkWidget *widget, gpointer data)
{
    gchar *filename = static_cast<gchar*>(data);

    GError *error = nullptr;
    GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(filename, &error);
    if (pixbuf == nullptr)
    {
        GtkWidget *error_dialog = gtk_message_dialog_new(GTK_WINDOW(window),
                                                        GTK_DIALOG_DESTROY_WITH_PARENT,
                                                        GTK_MESSAGE_ERROR,
                                                        GTK_BUTTONS_CLOSE,
                                                        "Error opening image: %s",
                                                        error->message);
        gtk_dialog_run(GTK_DIALOG(error_dialog));
        gtk_widget_destroy(error_dialog);
        g_error_free(error);
    }
    else
    {
        if (current_pixbuf != nullptr)
            g_object_unref(current_pixbuf);
        current_pixbuf = pixbuf;
        zoom_factor = 1.0;
        update_image_viewer();
        g_clear_pointer(&current_filename, g_free);
        current_filename = g_strdup(filename);
    }

    g_free(data);
}

// Function to load thumbnails asynchronously
static void load_thumbnails_async(const gchar *directory)
{
    std::thread([directory]() {
        create_thumbnails(directory);
    }).detach();
}

// Main function
int main(int argc, char *argv[])
{
    // Initialize GTK
    gtk_init(&argc, &argv);

    // Create the main window
    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    gtk_window_set_title(GTK_WINDOW(window), "Image Viewer");
    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);

    // Create the main layout
    GtkWidget *main_layout = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), main_layout);

    // Create the menu bar
    GtkWidget *menubar = gtk_menu_bar_new();
    gtk_box_pack_start(GTK_BOX(main_layout), menubar, FALSE, FALSE, 0);

    // Create the File menu
    GtkWidget *file_menu = gtk_menu_new();
    GtkWidget *file_menu_item = create_menu_item("_File", nullptr, nullptr, nullptr, nullptr, nullptr);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_menu_item), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file_menu_item);

    // Create menu items for File menu
    GtkWidget *open_menu_item = create_menu_item("_Open", "document-open", G_CALLBACK(open_menu_item_clicked), window, "O", "Open an image file");
    GtkWidget *save_menu_item = create_menu_item("_Save", "document-save", G_CALLBACK(save_menu_item_clicked), window, "S", "Save the current image");
    GtkWidget *quit_menu_item = create_menu_item("_Quit", "application-exit", G_CALLBACK(gtk_main_quit), nullptr, "Q", "Quit the application");

    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), open_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), save_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), quit_menu_item);

    // Create the View menu
    GtkWidget *view_menu = gtk_menu_new();
    GtkWidget *view_menu_item = create_menu_item("_View", nullptr, nullptr, nullptr, nullptr, nullptr);
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(view_menu_item), view_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), view_menu_item);

    // Create menu items for View menu
    GtkWidget *zoom_in_menu_item = create_menu_item("Zoom _In", "zoom-in", G_CALLBACK(zoom_in_menu_item_clicked), nullptr, "+", "Zoom in");
    GtkWidget *zoom_out_menu_item = create_menu_item("Zoom _Out", "zoom-out", G_CALLBACK(zoom_out_menu_item_clicked), nullptr, "-", "Zoom out");
    GtkWidget *reset_zoom_menu_item = create_menu_item("_Reset Zoom", "zoom-original", G_CALLBACK(reset_zoom_menu_item_clicked), nullptr, "0", "Reset zoom level");

    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), zoom_in_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), zoom_out_menu_item);
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), reset_zoom_menu_item);

    // Create the scrollable container
    GtkWidget *scrollable = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(scrollable), GTK_POLICY_AUTOMATIC, GTK_POLICY_AUTOMATIC);
    gtk_box_pack_start(GTK_BOX(main_layout), scrollable, TRUE, TRUE, 0);

    // Create the image viewer
    image_viewer = gtk_image_new();
    gtk_container_add(GTK_CONTAINER(scrollable), image_viewer);

    // Create the thumbnails box
    thumbnails_box = gtk_box_new(GTK_ORIENTATION_HORIZONTAL, 4);
    gtk_box_set_homogeneous(GTK_BOX(thumbnails_box), TRUE);

    // Create the thumbnails scroller
    thumbnails_scroller = gtk_scrolled_window_new(NULL, NULL);
    gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(thumbnails_scroller), GTK_POLICY_AUTOMATIC, GTK_POLICY_NEVER);
    gtk_scrolled_window_set_min_content_width(GTK_SCROLLED_WINDOW(thumbnails_scroller), 80 * thumbnail_count);

    gtk_container_add(GTK_CONTAINER(thumbnails_scroller), thumbnails_box);
    gtk_box_pack_start(GTK_BOX(main_layout), thumbnails_scroller, FALSE, FALSE, 0);

    // Connect key press event handler
    gtk_widget_add_events(window, GDK_KEY_PRESS_MASK);
    g_signal_connect(window, "key-press-event", G_CALLBACK(on_key_press), NULL);

    // Show the main window
    gtk_widget_show_all(window);

    // Start the GTK main loop
    gtk_main();

    // Clean up resources
    if (current_pixbuf != nullptr)
        g_object_unref(current_pixbuf);
    g_clear_pointer(&current_filename, g_free);

    return 0;
}
