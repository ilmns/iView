#include <gtk/gtk.h>
#include <gdk-pixbuf/gdk-pixbuf.h>

// Global variables
static GtkWidget *window;
static GtkWidget *image_viewer; // Image viewer widget
static GdkPixbuf *current_pixbuf = NULL; // The current image being displayed
static double zoom_factor = 1.0; // The current zoom level (1.0 = 100%)

// Function to update the image in the viewer
static void update_image()
{
    if (current_pixbuf != NULL)
    {
        int width = gdk_pixbuf_get_width(current_pixbuf);
        int height = gdk_pixbuf_get_height(current_pixbuf);
        int window_width, window_height;
        gtk_window_get_size(GTK_WINDOW(window), &window_width, &window_height);

        double window_aspect = (double)window_width / window_height;
        double image_aspect = (double)width / height;

        if (window_aspect > image_aspect)
        {
            zoom_factor = (double)window_height / height;
        }
        else
        {
            zoom_factor = (double)window_width / width;
        }

        GdkPixbuf *scaled_pixbuf = gdk_pixbuf_scale_simple(current_pixbuf, width * zoom_factor, height * zoom_factor, GDK_INTERP_BILINEAR);
        gtk_image_set_from_pixbuf(GTK_IMAGE(image_viewer), scaled_pixbuf);
        g_object_unref(scaled_pixbuf);
    }
    else
    {
        gtk_image_clear(GTK_IMAGE(image_viewer));
    }
}

// Function called when the "Zoom In" menu item is clicked
static void zoom_in_menu_item_clicked(GtkWidget *widget, gpointer data)
{
    zoom_factor *= 1.1;
    update_image();
}

// Function called when the "Zoom Out" menu item is clicked
static void zoom_out_menu_item_clicked(GtkWidget *widget, gpointer data)
{
    zoom_factor /= 1.1;
    update_image();
}

// Function called when the "Open" menu item is clicked
static void open_menu_item_clicked(GtkWidget *widget, gpointer data)
{
    GtkWidget *dialog = gtk_file_chooser_dialog_new(
        "Open File", GTK_WINDOW(data), GTK_FILE_CHOOSER_ACTION_OPEN,
        "Cancel", GTK_RESPONSE_CANCEL,
        "Open", GTK_RESPONSE_ACCEPT,
        NULL);

    GtkFileFilter *filter = gtk_file_filter_new();
    gtk_file_filter_set_name(filter, "Images");
    gtk_file_filter_add_mime_type(filter, "image/*");
    gtk_file_chooser_add_filter(GTK_FILE_CHOOSER(dialog), filter);

    gint response = gtk_dialog_run(GTK_DIALOG(dialog));

    if (response == GTK_RESPONSE_ACCEPT)
    {
        char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

        GError *error = NULL;
        GdkPixbuf *pixbuf = gdk_pixbuf_new_from_file(filename, &error);
        if (pixbuf == NULL)
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
            if (current_pixbuf != NULL)
            {
                g_object_unref(current_pixbuf);
            }
            current_pixbuf = pixbuf;
            zoom_factor = 1.0;
            update_image();
        }

        g_free(filename);
    }

    gtk_widget_destroy(dialog);
}

// Function called when the "Save" menu item is clicked
static void save_menu_item_clicked(GtkWidget *widget, gpointer data)
{
    if (current_pixbuf != NULL)
    {
        GtkWidget *dialog = gtk_file_chooser_dialog_new(
            "Save Image", GTK_WINDOW(data), GTK_FILE_CHOOSER_ACTION_SAVE,
            "Cancel", GTK_RESPONSE_CANCEL,
            "Save", GTK_RESPONSE_ACCEPT,
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

        gint response = gtk_dialog_run(GTK_DIALOG(dialog));

        if (response == GTK_RESPONSE_ACCEPT)
        {
            char *filename = gtk_file_chooser_get_filename(GTK_FILE_CHOOSER(dialog));

            GError *error = NULL;
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

// Function called when the "Rotate Clockwise" menu item is clicked
static void rotate_clockwise_menu_item_clicked(GtkWidget *widget, gpointer data)
{
    if (current_pixbuf != NULL)
    {
        GdkPixbuf *rotated_pixbuf = gdk_pixbuf_rotate_simple(current_pixbuf, GDK_PIXBUF_ROTATE_CLOCKWISE);
        g_object_unref(current_pixbuf);
        current_pixbuf = rotated_pixbuf;
        update_image();
    }
}

// Function called when the "Rotate Counterclockwise" menu item is clicked
static void rotate_counterclockwise_menu_item_clicked(GtkWidget *widget, gpointer data)
{
    if (current_pixbuf != NULL)
    {
        GdkPixbuf *rotated_pixbuf = gdk_pixbuf_rotate_simple(current_pixbuf, GDK_PIXBUF_ROTATE_COUNTERCLOCKWISE);
        g_object_unref(current_pixbuf);
        current_pixbuf = rotated_pixbuf;
        update_image();
    }
}

// Function called when the "Flip Horizontally" menu item is clicked
static void flip_horizontally_menu_item_clicked(GtkWidget *widget, gpointer data)
{
    if (current_pixbuf != NULL)
    {
        GdkPixbuf *flipped_pixbuf = gdk_pixbuf_flip(current_pixbuf, TRUE);
        g_object_unref(current_pixbuf);
        current_pixbuf = flipped_pixbuf;
        update_image();
    }
}

// Function called when the "Flip Vertically" menu item is clicked
static void flip_vertically_menu_item_clicked(GtkWidget *widget, gpointer data)
{
    if (current_pixbuf != NULL)
    {
        GdkPixbuf *flipped_pixbuf = gdk_pixbuf_flip(current_pixbuf, FALSE);
        g_object_unref(current_pixbuf);
        current_pixbuf = flipped_pixbuf;
        update_image();
    }
}

// Function called when the "Reset Zoom" menu item is clicked
static void reset_zoom_menu_item_clicked(GtkWidget *widget, gpointer data)
{
    zoom_factor = 1.0;
    update_image();
}

// Function to create a menu item
static GtkWidget* create_menu_item(const char *label, GCallback callback, gpointer data, const char *accelerator)
{
    GtkWidget *item = gtk_menu_item_new_with_label(label);
    g_signal_connect(item, "activate", callback, data);
    if (accelerator != NULL)
    {
        GtkAccelGroup *accel_group = gtk_accel_group_new();
        gtk_window_add_accel_group(GTK_WINDOW(window), accel_group);
        gtk_widget_add_accelerator(item, "activate", accel_group, gdk_keyval_from_name(accelerator), GDK_CONTROL_MASK, GTK_ACCEL_VISIBLE);
    }
    return item;
}


// Function to create the menubar
static GtkWidget* create_menubar(GtkWidget *window, GtkWidget *image_viewer)
{
    GtkWidget *menubar = gtk_menu_bar_new();
    GtkWidget *file_menu = gtk_menu_new();
    GtkWidget *view_menu = gtk_menu_new();
    GtkWidget *image_menu = gtk_menu_new();

    GtkWidget *file_item = gtk_menu_item_new_with_label("File");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(file_item), file_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), file_item);

    GtkWidget *view_item = gtk_menu_item_new_with_label("View");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(view_item), view_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), view_item);

    GtkWidget *image_item = gtk_menu_item_new_with_label("Image");
    gtk_menu_item_set_submenu(GTK_MENU_ITEM(image_item), image_menu);
    gtk_menu_shell_append(GTK_MENU_SHELL(menubar), image_item);

    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), create_menu_item("Open", G_CALLBACK(open_menu_item_clicked), window, "O"));
    gtk_menu_shell_append(GTK_MENU_SHELL(file_menu), create_menu_item("Save", G_CALLBACK(save_menu_item_clicked), window, "S"));

    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), create_menu_item("Zoom In", G_CALLBACK(zoom_in_menu_item_clicked), image_viewer, "+"));
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), create_menu_item("Zoom Out", G_CALLBACK(zoom_out_menu_item_clicked), image_viewer, "-"));
    gtk_menu_shell_append(GTK_MENU_SHELL(view_menu), create_menu_item("Reset Zoom", G_CALLBACK(reset_zoom_menu_item_clicked), image_viewer, "0"));

    gtk_menu_shell_append(GTK_MENU_SHELL(image_menu), create_menu_item("Rotate Clockwise", G_CALLBACK(rotate_clockwise_menu_item_clicked), image_viewer, "R"));
    gtk_menu_shell_append(GTK_MENU_SHELL(image_menu), create_menu_item("Rotate Counterclockwise", G_CALLBACK(rotate_counterclockwise_menu_item_clicked), image_viewer, "L"));
    gtk_menu_shell_append(GTK_MENU_SHELL(image_menu), create_menu_item("Flip Horizontally", G_CALLBACK(flip_horizontally_menu_item_clicked), image_viewer, "H"));
    gtk_menu_shell_append(GTK_MENU_SHELL(image_menu), create_menu_item("Flip Vertically", G_CALLBACK(flip_vertically_menu_item_clicked), image_viewer, "V"));

    return menubar;
}

// Main function
int main(int argc, char *argv[])
{
    gtk_init(&argc, &argv);

    window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
    g_signal_connect(window, "destroy", G_CALLBACK(gtk_main_quit), NULL);
    g_signal_connect(window, "size-allocate", G_CALLBACK(update_image), NULL);

    GtkWidget *vbox = gtk_box_new(GTK_ORIENTATION_VERTICAL, 0);
    gtk_container_add(GTK_CONTAINER(window), vbox);

    image_viewer = gtk_image_new();
    GtkWidget *scrollable = gtk_scrolled_window_new(NULL, NULL);
    gtk_container_add(GTK_CONTAINER(scrollable), image_viewer);
    gtk_box_pack_start(GTK_BOX(vbox), create_menubar(window, image_viewer), FALSE, FALSE, 0);
    gtk_box_pack_start(GTK_BOX(vbox), scrollable, TRUE, TRUE, 0);

    gtk_window_set_default_size(GTK_WINDOW(window), 800, 600);
    gtk_widget_show_all(window);

    gtk_main();

    if (current_pixbuf != NULL)
    {
        g_object_unref(current_pixbuf);
    }

    return 0;
}
