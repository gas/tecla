/* Copyright (C) 2023 Red Hat, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, see <http://www.gnu.org/licenses/>.
 *
 * Author: Carlos Garnacho <carlosg@gnome.org>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "tecla-key.h"

#include <math.h>

struct _TeclaKey
{
	GtkWidget parent_class;
	gchar *name;
	gchar *label;
	gchar *label_altgr; // Nueva etiqueta para AltGr
};

enum
{
	PROP_0,
	PROP_NAME,
	PROP_LABEL,
    // PROP_LABEL_ALTGR, // Podríamos añadir una propiedad si fuera necesario
	N_PROPS,
};

static GParamSpec *props[N_PROPS] = { 0, };

enum
{
	ACTIVATED,
	N_SIGNALS,
};

static guint signals[N_SIGNALS] = { 0, };

G_DEFINE_TYPE (TeclaKey, tecla_key, GTK_TYPE_WIDGET)

static void
tecla_key_get_property (GObject    *object,
			guint       prop_id,
			GValue     *value,
			GParamSpec *pspec)
{
	TeclaKey *key = TECLA_KEY (object);

	switch (prop_id) {
	case PROP_NAME:
		g_value_set_string (value, key->name);
		break;
	case PROP_LABEL:
		g_value_set_string (value, key->label);
		break;
    // case PROP_LABEL_ALTGR: // Si se define como propiedad
	//	g_value_set_string (value, key->label_altgr);
	//	break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tecla_key_set_property (GObject      *object,
			guint         prop_id,
			const GValue *value,
			GParamSpec   *pspec)
{
	TeclaKey *key = TECLA_KEY (object);

	switch (prop_id) {
	case PROP_NAME:
		key->name = g_value_dup_string (value);
		break;
	case PROP_LABEL:
		tecla_key_set_label (TECLA_KEY (object),
				     g_value_get_string (value));
		break;
    // case PROP_LABEL_ALTGR: // Si se define como propiedad
	//	tecla_key_set_label_altgr (TECLA_KEY (object),
	//				     g_value_get_string (value));
	//	break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
	}
}

static void
tecla_key_finalize (GObject *object)
{
	TeclaKey *key = TECLA_KEY (object);

	g_free (key->name);
	g_free (key->label);
	g_free (key->label_altgr); // Liberar la nueva etiqueta

	G_OBJECT_CLASS (tecla_key_parent_class)->finalize (object);
}

static void
tecla_key_snapshot (GtkWidget *widget,
		    GtkSnapshot *snapshot)
{
	TeclaKey *key = TECLA_KEY (widget);
	PangoLayout *layout_main = NULL, *layout_altgr = NULL;
	PangoRectangle rect_main, rect_altgr;
	GdkRGBA color_main;
	GdkRGBA color_altgr = {1, 0, 0, 1}; // Rojo para AltGr
	int width, height;
	float scale_main, scale_altgr;

	width = gtk_widget_get_width (widget);
	height = gtk_widget_get_height (widget);
	gtk_widget_get_color (widget, &color_main);

	// Etiqueta principal
	if (key->label && key->label[0] != '\0') {
		layout_main = gtk_widget_create_pango_layout (widget, key->label);
		pango_layout_get_pixel_extents (layout_main, NULL, &rect_main);
		// Escalar para que quepa, intentando que no sea demasiado pequeño.
		// Podríamos querer un tamaño de fuente ligeramente más pequeño si ambas etiquetas están presentes.
        float height_ratio_main = (key->label_altgr && key->label_altgr[0] != '\0') ? 0.60f : 0.75f;
		scale_main = MIN ((float) (height * height_ratio_main) / rect_main.height, 2.0f);
        scale_main = MAX (scale_main, 0.5f); // Evitar que sea demasiado pequeño
		scale_main = roundf (scale_main * 4.0f) / 4.0f; // Ajustar a cuartos de píxel

        // Posición para la etiqueta principal (ej: arriba-centro o centro-izquierda)
        // Si hay etiqueta altgr, mover esta un poco hacia arriba, sino centrada
        int x_main = (width / 2) - ((rect_main.width / 2) * scale_main);
        int y_main;
        if (key->label_altgr && key->label_altgr[0] != '\0') {
             y_main = (height * 0.25f) - ((rect_main.height / 2) * scale_main) ; // Cuarto superior
        } else {
             y_main = (height / 2) - ((rect_main.height / 2) * scale_main); // Centrada
        }
        y_main = MAX(y_main, 0); // Evitar que se salga por arriba


		gtk_snapshot_save (snapshot);
		gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x_main, y_main));
		gtk_snapshot_scale (snapshot, scale_main, scale_main);
		gtk_snapshot_append_layout (snapshot, layout_main, &color_main);
		gtk_snapshot_restore (snapshot);
		g_object_unref (layout_main);
	}

	// Etiqueta AltGr (secundaria)
	if (key->label_altgr && key->label_altgr[0] != '\0') {
		layout_altgr = gtk_widget_create_pango_layout (widget, key->label_altgr);
		pango_layout_get_pixel_extents (layout_altgr, NULL, &rect_altgr);
        float height_ratio_altgr = 0.60f; // Darle un poco menos de espacio si la principal existe
		scale_altgr = MIN ((float) (height * height_ratio_altgr) / rect_altgr.height, 2.0f);
        scale_altgr = MAX (scale_altgr, 0.5f);
		scale_altgr = roundf (scale_altgr * 4.0f) / 4.0f;

        // Posición para la etiqueta AltGr (ej: abajo-centro o centro-derecha)
        int x_altgr = (width / 2) - ((rect_altgr.width / 2) * scale_altgr);
        int y_altgr = (height * 0.75f) - ((rect_altgr.height / 2) * scale_altgr); // Tres cuartos inferiores
        y_altgr = MAX(y_altgr, (int)(height * 0.5f)); // Evitar que se solape demasiado con la de arriba

		gtk_snapshot_save (snapshot);
		gtk_snapshot_translate (snapshot, &GRAPHENE_POINT_INIT (x_altgr, y_altgr));
		gtk_snapshot_scale (snapshot, scale_altgr, scale_altgr);
		gtk_snapshot_append_layout (snapshot, layout_altgr, &color_altgr);
		gtk_snapshot_restore (snapshot);
		g_object_unref (layout_altgr);
	}
}


static void
tecla_key_class_init (TeclaKeyClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);
	GtkCssProvider *css_provider;

	object_class->set_property = tecla_key_set_property;
	object_class->get_property = tecla_key_get_property;
	object_class->finalize = tecla_key_finalize;

	widget_class->snapshot = tecla_key_snapshot;

	signals[ACTIVATED] =
		g_signal_new ("activated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      0,
			      NULL, NULL, NULL,
			      G_TYPE_NONE, 0);

	props[PROP_NAME] =
		g_param_spec_string ("name",
				     "name",
				     "name",
				     NULL,
				     G_PARAM_READWRITE |
				     G_PARAM_CONSTRUCT_ONLY |
				     G_PARAM_STATIC_STRINGS);
	props[PROP_LABEL] =
		g_param_spec_string ("label",
				     "label",
				     "label",
				     NULL,
				     G_PARAM_READWRITE |
				     G_PARAM_STATIC_STRINGS);
    // Podríamos añadir PROP_LABEL_ALTGR aquí si quisiéramos que fuera una propiedad completa.
    // Por ahora, solo con el setter es suficiente para el propósito.

	g_object_class_install_properties (object_class, N_PROPS, props);

	css_provider = gtk_css_provider_new ();
	gtk_css_provider_load_from_resource (css_provider,
					     "/org/gnome/tecla/tecla-key.css");
	gtk_style_context_add_provider_for_display (gdk_display_get_default (),
						    GTK_STYLE_PROVIDER (css_provider),
						    GTK_STYLE_PROVIDER_PRIORITY_APPLICATION);

	gtk_widget_class_set_css_name (widget_class, "button");
}

static void
click_release_cb (GtkGestureClick *gesture,
		  int              n_press,
		  double           x,
		  double           y,
		  TeclaKey        *key)
{
	g_signal_emit (key, signals[ACTIVATED], 0);
}

static void
tecla_key_init (TeclaKey *key)
{
	GtkGesture *gesture;

	gesture = gtk_gesture_click_new ();
	g_signal_connect (gesture, "released",
			  G_CALLBACK (click_release_cb), key);

	gtk_widget_add_controller (GTK_WIDGET (key),
				   GTK_EVENT_CONTROLLER (gesture));
	gtk_widget_add_css_class (GTK_WIDGET (key), "opaque");
}

GtkWidget *
tecla_key_new (const gchar *name)
{
	return g_object_new (TECLA_TYPE_KEY,
			     "name", name,
			     NULL);
}

void
tecla_key_set_label (TeclaKey    *key,
		     const gchar *label)
{
	if (g_strcmp0 (label, key->label) == 0)
		return;

	g_free (key->label);
	key->label = g_strdup (label);
	gtk_widget_queue_draw (GTK_WIDGET (key));
}

// Implementación de la nueva función para la etiqueta AltGr
void
tecla_key_set_label_altgr (TeclaKey    *key,
                           const gchar *label_altgr)
{
    if (g_strcmp0 (label_altgr, key->label_altgr) == 0)
        return;

    g_free (key->label_altgr);
    key->label_altgr = g_strdup (label_altgr);
    gtk_widget_queue_draw (GTK_WIDGET (key));
}

const gchar *
tecla_key_get_name (TeclaKey *key)
{
	return key->name;
}
