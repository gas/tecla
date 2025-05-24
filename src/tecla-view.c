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

#include <gtk/gtk.h>
#include <xkbcommon/xkbcommon.h>

#include "tecla-view.h"

#include "ansi104.h"
#include "tecla-key.h"

enum
{
	LEVEL2_PRESSED = 1 << 0,
	LEVEL3_PRESSED = 1 << 1,
};

struct _TeclaView
{
	GtkWidget parent_instance;
	GtkWidget *grid;
	GHashTable *keys_by_name;
	TeclaModel *model;
	guint model_changed_id;

	GList *level2_keys; // Shift keys
	GList *level3_keys; // AltGr keys
	guint toggled_levels;
	int level; // 0: base, 1: shift, 2: altgr, 3: shift+altgr
};

G_DEFINE_TYPE (TeclaView, tecla_view, GTK_TYPE_WIDGET)

enum
{
	PROP_0,
	PROP_MODEL,
	PROP_LEVEL,
	PROP_NUM_LEVELS,
	N_PROPS,
};

static GParamSpec *props[N_PROPS];

enum
{
	KEY_ACTIVATED,
	N_SIGNALS,
};

static guint signals[N_SIGNALS] = { 0, };

static void update_view (TeclaView *view);

static void
tecla_view_set_property (GObject      *object,
			 guint         prop_id,
			 const GValue *value,
			 GParamSpec   *pspec)
{
	TeclaView *view = TECLA_VIEW (object);

	switch (prop_id) {
	case PROP_MODEL:
		tecla_view_set_model (view, g_value_get_object (value));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tecla_view_get_property (GObject    *object,
			 guint       prop_id,
			 GValue     *value,
			 GParamSpec *pspec)
{
	TeclaView *view = TECLA_VIEW (object);

	switch (prop_id) {
	case PROP_MODEL:
		g_value_set_object (value, view->model);
		break;
	case PROP_LEVEL:
		g_value_set_int (value, view->level);
		break;
	case PROP_NUM_LEVELS:
		g_value_set_int (value, tecla_view_get_num_levels (view));
		break;
	default:
		G_OBJECT_WARN_INVALID_PROPERTY_ID (object, prop_id, pspec);
		break;
	}
}

static void
tecla_view_finalize (GObject *object)
{
	TeclaView *view = TECLA_VIEW (object);

	g_hash_table_unref (view->keys_by_name);
	g_clear_list (&view->level2_keys, NULL);
	g_clear_list (&view->level3_keys, NULL);
	gtk_widget_unparent (gtk_widget_get_first_child (GTK_WIDGET (view)));

	G_OBJECT_CLASS (tecla_view_parent_class)->finalize (object);
}

static void
update_toggled_key_list (TeclaView *view,
			 GList     *keys,
			 guint      flag)
{
	GList *l;

	for (l = keys; l; l = l->next) {
		GtkWidget *key;

		key = g_hash_table_lookup (view->keys_by_name, l->data);

		if ((view->toggled_levels & flag) != 0)
			gtk_widget_set_state_flags (key, GTK_STATE_FLAG_SELECTED, FALSE);
		else
			gtk_widget_unset_state_flags (key, GTK_STATE_FLAG_SELECTED);
	}
}

static void
update_toggled_keys (TeclaView   *view,
		     const gchar *pressed_key_name)
{
	const gchar *name = pressed_key_name;

	if (g_list_find_custom (view->level2_keys, name, (GCompareFunc) g_strcmp0)) {
		if ((view->toggled_levels & LEVEL2_PRESSED) != 0)
			view->toggled_levels &= ~LEVEL2_PRESSED;
		else
			view->toggled_levels |= LEVEL2_PRESSED;
	} else if (g_list_find_custom (view->level3_keys, name, (GCompareFunc) g_strcmp0)) {
		if ((view->toggled_levels & LEVEL3_PRESSED) != 0)
			view->toggled_levels &= ~LEVEL3_PRESSED;
		else
			view->toggled_levels |= LEVEL3_PRESSED;
	}

	update_toggled_key_list (view, view->level2_keys, LEVEL2_PRESSED);
	update_toggled_key_list (view, view->level3_keys, LEVEL3_PRESSED);
}

static void
update_level (TeclaView *view)
{
	int level;

	if (view->toggled_levels == (LEVEL2_PRESSED | LEVEL3_PRESSED))
		level = 3;
	else if (view->toggled_levels == LEVEL3_PRESSED)
		level = 2;
	else if (view->toggled_levels == LEVEL2_PRESSED)
		level = 1;
	else
		level = 0;

	if (view->level == level)
		return;

	view->level = level;
	g_object_notify (G_OBJECT (view), "level");
	update_view (view);
}

static void
bind_state (GtkWidget     *w1,
	    GtkStateFlags  old_flags,
	    GtkWidget     *w2)
{
	GtkStateFlags flags;

	flags = gtk_widget_get_state_flags (w1);

	if (flags != gtk_widget_get_state_flags (w2))
		gtk_widget_set_state_flags (w2, flags, TRUE);
}

static void
pair_state (GtkWidget *widget,
	    GtkWidget *other_widget)
{
	g_signal_connect (widget, "state-flags-changed",
			  G_CALLBACK (bind_state), other_widget);
	g_signal_connect (other_widget, "state-flags-changed",
			  G_CALLBACK (bind_state), widget);
        g_object_bind_property (other_widget, "label",
                                widget, "label",
                                G_BINDING_BIDIRECTIONAL | G_BINDING_SYNC_CREATE);
}

static void
key_activated_cb (TeclaKey  *key,
		  TeclaView *view)
{
	const gchar *name;

	name = tecla_key_get_name (key);
	g_signal_emit (view, signals[KEY_ACTIVATED], 0, name, key);

	update_toggled_keys (view, name);
	update_level (view);
}

static void
construct_grid (TeclaView *view)
{
	gulong i, j;
	int anchor = 0;

	/* make sure we show the keyboard layout in RTL same as in LTR */
	gtk_widget_set_direction (view->grid, GTK_TEXT_DIR_LTR);

	for (i = 0; i < G_N_ELEMENTS (pc105_layout.rows); i++) {
		for (j = 0; j < G_N_ELEMENTS (pc105_layout.rows[i].keys); j++) {
			TeclaLayoutKey *key;
			GtkWidget *button, *prev;
			double width, height;
			int left, top;

			key = &pc105_layout.rows[i].keys[j];
			if (!key->name)
				break;

			left = anchor;
			top = key->height >= 0 ? i : i + key->height + 1;
			width = MAX (key->width, 1) * 4;
			height = MAX (fabs (key->height), 1);

			button = tecla_key_new (key->name);
			g_signal_connect (button, "activated",
					  G_CALLBACK (key_activated_cb), view);

			gtk_widget_add_css_class (button, "tecla-key");
			gtk_grid_attach (GTK_GRID (view->grid), button,
					 left, top,
					 (int) width,
					 (int) height);

			anchor += (int) width;

			prev = g_hash_table_lookup (view->keys_by_name,
						    key->name);

			if (prev) {
				pair_state (prev, button);
			} else {
				g_hash_table_insert (view->keys_by_name,
						     (gpointer) tecla_key_get_name (TECLA_KEY (button)),
						     button);
			}
		}

		anchor = 0;
	}

	gtk_widget_set_layout_manager (GTK_WIDGET (view), gtk_bin_layout_new ());
}

static void
tecla_view_constructed (GObject *object)
{
	TeclaView *view = TECLA_VIEW (object);

	G_OBJECT_CLASS (tecla_view_parent_class)->constructed (object);

	construct_grid (view);
}

static void
tecla_view_class_init (TeclaViewClass *klass)
{
	GObjectClass *object_class = G_OBJECT_CLASS (klass);
	GtkWidgetClass *widget_class = GTK_WIDGET_CLASS (klass);

	object_class->set_property = tecla_view_set_property;
	object_class->get_property = tecla_view_get_property;
	object_class->finalize = tecla_view_finalize;
	object_class->constructed = tecla_view_constructed;

	signals[KEY_ACTIVATED] =
		g_signal_new ("key-activated",
			      G_OBJECT_CLASS_TYPE (object_class),
			      G_SIGNAL_RUN_LAST,
			      0, NULL, NULL, NULL,
			      G_TYPE_NONE,
			      2, G_TYPE_STRING, GTK_TYPE_WIDGET);

	props[PROP_MODEL] =
		g_param_spec_object ("model",
				     "Model",
				     "Model",
				     TECLA_TYPE_MODEL,
				     G_PARAM_READWRITE |
				     G_PARAM_CONSTRUCT_ONLY);
	props[PROP_LEVEL] =
		g_param_spec_int ("level",
				  "Level",
				  "Level",
				  0, G_MAXINT, 0,
				  G_PARAM_READABLE);
	props[PROP_NUM_LEVELS] =
		g_param_spec_int ("num-levels",
				  "Number of levels",
				  "Number of levels",
				  0, G_MAXINT, 0,
				  G_PARAM_READABLE);

	g_object_class_install_properties (object_class, N_PROPS, props);

	gtk_widget_class_set_template_from_resource (widget_class, "/org/gnome/tecla/tecla-view.ui");
	gtk_widget_class_bind_template_child (widget_class, TeclaView, grid);
}

static void
key_pressed_cb (GtkEventControllerKey *controller,
		guint                  keyval,
		guint                  keycode,
		GdkModifierType        modifiers,
		TeclaView             *view)
{
	const gchar *name;
	GtkWidget *key;

	if (!view->model)
		return;

	name = tecla_model_get_keycode_key (view->model, keycode);
	key = g_hash_table_lookup (view->keys_by_name, name);

	if (key)
		gtk_widget_set_state_flags (key, GTK_STATE_FLAG_ACTIVE, FALSE);

	update_toggled_keys (view, name);
	update_level (view);
}

static void
key_released_cb (GtkEventControllerKey *controller,
		 guint                  keyval,
		 guint                  keycode,
		 GdkModifierType        modifiers,
		 TeclaView             *view)
{
	const gchar *name;
	GtkWidget *key;

	if (!view->model)
		return;

	name = tecla_model_get_keycode_key (view->model, keycode);
	key = g_hash_table_lookup (view->keys_by_name, name);

	if (key)
		gtk_widget_unset_state_flags (key, GTK_STATE_FLAG_ACTIVE);

	g_signal_emit (view, signals[KEY_ACTIVATED], 0, name, key);
}

static void
tecla_view_init (TeclaView *view)
{
	GtkEventController *controller;

	gtk_widget_init_template (GTK_WIDGET (view));
	view->keys_by_name = g_hash_table_new (g_str_hash, g_str_equal);

	controller = gtk_event_controller_key_new ();
	g_signal_connect (controller, "key-pressed",
			  G_CALLBACK (key_pressed_cb), view);
	g_signal_connect (controller, "key-released",
			  G_CALLBACK (key_released_cb), view);
	gtk_widget_add_controller (GTK_WIDGET (view), controller);

	gtk_widget_set_focusable (GTK_WIDGET (view), TRUE);
}

static void
update_from_model_foreach (const gchar *name,
			   GtkWidget   *key_widget, // Cambiado de TeclaKey* a GtkWidget* para que coincida con GHFunc
			   TeclaView   *view)
{
	TeclaKey *key = TECLA_KEY(key_widget); // Hacer el cast aquí
	xkb_keycode_t keycode;
	g_autofree gchar *action_main = NULL;
	g_autofree gchar *action_altgr = NULL;
	guint keyval_main;
    int altgr_paired_level;

	if (!view->model) { // Añadir una guarda por si el modelo no está listo
		tecla_key_set_label(key, "");
		tecla_key_set_label_altgr(key, "");
		return;
	}

	keycode = tecla_model_get_key_keycode (view->model, name);
	keyval_main = tecla_model_get_keyval (view->model, 0, keycode); // Nivel 0 para identificar Shift/AltGr

    // Identificar teclas Shift y AltGr para la lógica de `view->level2_keys` y `view->level3_keys`
    // Esta parte es para la gestión interna de niveles, no para las etiquetas en sí.
	if (keyval_main == GDK_KEY_Shift_L || keyval_main == GDK_KEY_Shift_R) {
		if (!g_list_find_custom (view->level2_keys, name, (GCompareFunc) g_strcmp0))
			view->level2_keys = g_list_prepend (view->level2_keys, (gpointer) name);
		action_main = g_strdup ("⬆");
	} else if (keyval_main == GDK_KEY_ISO_Level3_Shift) { // Esta es la tecla AltGr
		if (!g_list_find_custom (view->level3_keys, name, (GCompareFunc) g_strcmp0))
			view->level3_keys = g_list_prepend (view->level3_keys, (gpointer) name);
		action_main = g_strdup ("⎇");
	}


    // Obtener etiqueta principal (nivel base o nivel Shift)
    // view->level ya refleja el estado actual de los modificadores (0:base, 1:Shift, 2:AltGr, 3:Shift+AltGr)
    // Para la etiqueta principal, nos interesan los niveles 0 y 1.
    // Si AltGr está pulsado (level 2 o 3), la etiqueta principal debería seguir siendo la de nivel 0 o 1.
    // O, si queremos que la etiqueta principal refleje el estado de Shift incluso con AltGr:
    int main_label_level = view->level % 2; // 0 si level es 0 o 2; 1 si level es 1 o 3. Esto es (Base o AltGr) y (Shift o Shift+AltGr)


    if (!action_main) { // Si no es una tecla especial como Shift o AltGr
        action_main = tecla_model_get_key_label (view->model, main_label_level, name);
    }


	// Obtener etiqueta AltGr (nivel AltGr o nivel Shift+AltGr)
    // Determinamos el nivel para la etiqueta AltGr basado en si Shift está presionado o no.
    if ((view->toggled_levels & LEVEL2_PRESSED) != 0) { // Shift está presionado
        altgr_paired_level = 3; // Nivel Shift + AltGr
    } else { // Shift no está presionado
        altgr_paired_level = 2; // Nivel AltGr
    }
    action_altgr = tecla_model_get_key_label(view->model, altgr_paired_level, name);

	tecla_key_set_label (key, action_main ? action_main : "");
    tecla_key_set_label_altgr (key, action_altgr ? action_altgr : "");

    // g_free(action_main) y g_free(action_altgr) son manejados por g_autofree
}

static void
update_view (TeclaView *view)
{
	if (!view->model) return; // Añadir guarda
	g_hash_table_foreach (view->keys_by_name,
			      (GHFunc) update_from_model_foreach,
			      view);
}

GtkWidget *
tecla_view_new (void)
{
	return g_object_new (TECLA_TYPE_VIEW, NULL);
}

static void
model_changed_cb (TeclaModel *model,
		  TeclaView  *view)
{
	// Limpiar las listas de teclas de nivel ANTES de llamar a update_view,
    // ya que update_from_model_foreach las repoblará.
	g_clear_list (&view->level2_keys, NULL);
	g_clear_list (&view->level3_keys, NULL);

	view->toggled_levels = 0;
	view->level = 0; // Resetear al nivel base
	update_toggled_key_list (view, view->level2_keys, LEVEL2_PRESSED); // Esto no hará nada si las listas están vacías
	update_toggled_key_list (view, view->level3_keys, LEVEL3_PRESSED); // ""
	// update_level (view); // Se llamará dentro de set_model o al final de esta función si es necesario.

	update_view (view); // Esto repoblará level2_keys/level3_keys y establecerá etiquetas

    // Después de update_view, los toggled_levels podrían haber cambiado si las teclas Shift/AltGr
    // se procesaron. Forzamos una actualización de nivel y notificamos.
    update_level(view); // Asegura que el nivel se recalcula y notifica si es necesario.


	g_object_notify (G_OBJECT (view), "num-levels"); // Notificar cambio en el número de niveles
	g_object_notify (G_OBJECT (view), "level");      // Notificar cambio de nivel
}

void
tecla_view_set_model (TeclaView  *view,
		      TeclaModel *model)
{
	if (view->model == model)
		return;

	if (view->model_changed_id) {
		g_signal_handler_disconnect (view->model, view->model_changed_id);
		view->model_changed_id = 0;
	}

	g_set_object (&view->model, model);

	if (view->model) {
		view->model_changed_id =
			g_signal_connect (view->model, "changed",
					  G_CALLBACK (model_changed_cb), view);
        model_changed_cb (view->model, view); // Llamar para la configuración inicial con el nuevo modelo
	} else {
        // Si el modelo se establece a NULL, limpiar la vista
        view->toggled_levels = 0;
        view->level = 0;
        g_clear_list (&view->level2_keys, NULL);
	    g_clear_list (&view->level3_keys, NULL);
        update_view(view); // Limpiará las etiquetas
        g_object_notify (G_OBJECT (view), "num-levels");
	    g_object_notify (G_OBJECT (view), "level");
    }
}

int
tecla_view_get_current_level (TeclaView *view)
{
	return view->level;
}

void
tecla_view_set_current_level (TeclaView *view,
			      int        level)
{
	guint toggled_levels = 0;

    // Esta función es llamada por los botones de nivel (1, 2, 3, 4)
    // El 'level' aquí es 0-indexed internamente por Tecla (0,1,2,3)
	if (level == 3) // Shift + AltGr
        toggled_levels = LEVEL2_PRESSED | LEVEL3_PRESSED;
    else if (level == 2) // AltGr
        toggled_levels = LEVEL3_PRESSED;
    else if (level == 1) // Shift
        toggled_levels = LEVEL2_PRESSED;
    // else level == 0 (base), toggled_levels = 0

	if (view->toggled_levels == toggled_levels) return;

	view->toggled_levels = toggled_levels;
	update_toggled_key_list (view, view->level2_keys, LEVEL2_PRESSED);
	update_toggled_key_list (view, view->level3_keys, LEVEL3_PRESSED);
	update_level (view); // Esto actualizará view->level y llamará a update_view
}

int
tecla_view_get_num_levels (TeclaView *view)
{
    // Esta lógica depende de si las teclas Shift y AltGr están presentes en el layout actual.
    // Es posible que el modelo aún no se haya cargado o no tenga estas teclas.
    if (!view->model) return 1;

    // Para ser más robusto, deberíamos basarnos en las capacidades del xkb_keymap.
    // Pero la lógica actual de Tecla usa la presencia de teclas especiales en el pc105_layout.
    // Para este cambio, mantenemos la lógica existente pero asegurándonos que
    // level2_keys y level3_keys se pueblan correctamente en update_from_model_foreach
    // incluso si no son parte del layout visual (ej. si el layout es muy minimalista).

	if (view->level2_keys && view->level3_keys) // Tiene Shift y AltGr
		return 4;
	else if (view->level3_keys) // Tiene solo AltGr (o AltGr y no Shift, raro pero posible)
		return 2; // Asumimos Base y AltGr
    else if (view->level2_keys) // Tiene solo Shift
        return 2; // Asumimos Base y Shift
	else // No tiene ni Shift ni AltGr detectados como modificadores de nivel
		return 1;
}
