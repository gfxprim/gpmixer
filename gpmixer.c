//SPDX-License-Identifier: LGPL-2.0-or-later

/*

    Copyright (C) 2007-2020 Cyril Hrubis <metan@ucw.cz>

 */

#include <alsa/asoundlib.h>
#include <widgets/gp_widgets.h>

struct elem_group {
	gp_widget *slider;
	gp_widget *choice;
	gp_widget *chbox;
};

struct elem_group *alloc_elem_groups(unsigned int count)
{
	struct elem_group *groups = malloc(sizeof(struct elem_group) * count);
	if (!count)
		return NULL;

	memset(groups, 0, sizeof(struct elem_group) * count);

	return groups;
}

static int slider_playback_callback(gp_widget_event *ev)
{
	long volume = ev->self->i->val;

	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	snd_mixer_selem_set_playback_volume_all(ev->self->priv, volume);

	return 0;
}

static int mixer_playback_callback(snd_mixer_elem_t *elem,
                          unsigned int mask __attribute__((unused)))
{
	struct elem_group *grp = snd_mixer_elem_get_callback_private(elem);

	//TODO: Use mask?

	if (grp->slider) {
		long volume;

		snd_mixer_selem_get_playback_volume(elem, 0, &volume);
		gp_widget_int_set(grp->slider, volume);
	}

	if (grp->chbox) {
		int val;

		snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_MONO, &val);

		gp_widget_checkbox_set(grp->chbox, val);
	}

	if (grp->choice) {
		unsigned int sel;

		snd_mixer_selem_get_enum_item(elem, SND_MIXER_SCHN_MONO, &sel);

		gp_widget_choice_set(grp->choice, sel);
	}

	return 0;
}

static gp_widget *create_playback_slider(snd_mixer_elem_t *elem)
{
	long min, max, volume;
	gp_widget *slider;

	if (!snd_mixer_selem_has_playback_volume(elem))
		return NULL;

	snd_mixer_selem_get_playback_volume_range(elem, &min, &max);
	snd_mixer_selem_get_playback_volume(elem, 0, &volume);

	slider = gp_widget_slider_new(min, max, volume, GP_WIDGET_VERT,
	                              slider_playback_callback, elem);

	slider->align = GP_VFILL | GP_HCENTER;

	return slider;
}

static int chbox_playback_callback(gp_widget_event *ev)
{
	int val = ev->self->checkbox->val;

	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	snd_mixer_selem_set_playback_switch_all(ev->self->priv, val);

	return 0;
}

static gp_widget *create_playback_chbox(snd_mixer_elem_t *elem)
{
	int val;
	gp_widget *chbox;

	if (!snd_mixer_selem_has_playback_switch(elem))
		return NULL;

	snd_mixer_selem_get_playback_switch(elem, SND_MIXER_SCHN_MONO, &val);

	chbox = gp_widget_checkbox_new(NULL, val, chbox_playback_callback, elem);

	return chbox;
}

static gp_widget *create_label(snd_mixer_elem_t *elem)
{
	const char *name = snd_mixer_selem_get_name(elem);

	return gp_widget_label_new(name, 0, 0);
}

static int enum_playback_callback(gp_widget_event *ev)
{
	unsigned int sel = gp_widget_choice_get(ev->self);

	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	snd_mixer_selem_set_enum_item(ev->self->priv, SND_MIXER_SCHN_MONO, sel);

	return 0;
}

static gp_widget *create_playback_enum(snd_mixer_elem_t *elem)
{
	int i, n = snd_mixer_selem_get_enum_items(elem);
	char enums[n][64];
	const char *choices[n];
	unsigned int sel;

	for (i = 0; i < n; i++) {
		snd_mixer_selem_get_enum_item_name(elem, i, 64, enums[i]);
		choices[i] = enums[i];
	}

	snd_mixer_selem_get_enum_item(elem, SND_MIXER_SCHN_MONO, &sel);

	return gp_widget_radiobutton_new(choices, n, sel, enum_playback_callback, elem);
}

static int is_playback(snd_mixer_elem_t *elem)
{
	return snd_mixer_selem_has_playback_volume(elem) ||
	       snd_mixer_selem_has_playback_switch(elem) ||
	       snd_mixer_selem_is_enumerated(elem);
}

unsigned int playback_count(snd_mixer_t *mixer)
{
	unsigned int cnt = 0;
	snd_mixer_elem_t *elem = snd_mixer_first_elem(mixer);

	for (; elem != NULL; elem = snd_mixer_elem_next(elem)) {
		if (is_playback(elem))
			cnt++;
	}

	return cnt;
}

static gp_widget *create_playback_widgets(snd_mixer_t *mixer)
{
	snd_mixer_elem_t *elem = snd_mixer_first_elem(mixer);
	unsigned int i = 0, count = playback_count(mixer);

	gp_widget *playback = gp_widget_grid_new(count, 3, 0);

	struct elem_group *playback_groups = malloc(sizeof(struct elem_group) * count);
	if (!count)
		return playback;

	memset(playback_groups, 0, sizeof(struct elem_group) * count);

	playback->align = GP_VFILL | GP_HCENTER;

	for (; elem != NULL; elem = snd_mixer_elem_next(elem)) {
		if (!is_playback(elem))
			continue;

		if (snd_mixer_selem_is_enumerated(elem)) {
			playback_groups[i].choice = create_playback_enum(elem);
			gp_widget_grid_put(playback, i, 0, playback_groups[i].choice);
		} else {
			playback_groups[i].slider = create_playback_slider(elem);

			if (playback_groups[i].slider)
				gp_widget_grid_put(playback, i, 0, playback_groups[i].slider);
		}

		playback_groups[i].chbox = create_playback_chbox(elem);
		gp_widget_grid_put(playback, i, 1, playback_groups[i].chbox);

		gp_widget *label = create_label(elem);
		gp_widget_grid_put(playback, i, 2, label);

		snd_mixer_elem_set_callback_private(elem, &playback_groups[i]);
		snd_mixer_elem_set_callback(elem, mixer_playback_callback);

		i++;
	}

	return playback;
}

static int slider_capture_callback(gp_widget_event *ev)
{
	long volume = ev->self->i->val;

	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	snd_mixer_selem_set_capture_volume_all(ev->self->priv, volume);

	return 0;
}

static gp_widget *create_capture_slider(snd_mixer_elem_t *elem)
{
	long min, max, volume;
	gp_widget *slider;

	if (!snd_mixer_selem_has_capture_volume(elem))
		return NULL;

	snd_mixer_selem_get_capture_volume_range(elem, &min, &max);
	snd_mixer_selem_get_capture_volume(elem, 0, &volume);

	slider = gp_widget_slider_new(min, max, volume, GP_WIDGET_VERT,
	                              slider_capture_callback, elem);

	slider->align = GP_VFILL | GP_HCENTER;

	snd_mixer_elem_set_callback_private(elem, slider);
	snd_mixer_elem_set_callback(elem, mixer_playback_callback);

	return slider;
}

static int chbox_capture_callback(gp_widget_event *ev)
{
	int val = ev->self->checkbox->val;

	if (ev->type != GP_WIDGET_EVENT_WIDGET)
		return 0;

	snd_mixer_selem_set_capture_switch_all(ev->self->priv, val);

	return 0;
}

static gp_widget *create_capture_chbox(snd_mixer_elem_t *elem)
{
	int val;
	gp_widget *chbox;

	if (!snd_mixer_selem_has_capture_switch(elem))
		return NULL;

	snd_mixer_selem_get_capture_switch(elem, SND_MIXER_SCHN_MONO, &val);

	chbox = gp_widget_checkbox_new(NULL, val, chbox_capture_callback, elem);

	return chbox;
}

static int is_capture(snd_mixer_elem_t *elem)
{
	return snd_mixer_selem_has_capture_volume(elem) ||
	       snd_mixer_selem_has_capture_switch(elem);
}

unsigned int capture_count(snd_mixer_t *mixer)
{
	unsigned int cnt = 0;
	snd_mixer_elem_t *elem = snd_mixer_first_elem(mixer);

	for (; elem != NULL; elem = snd_mixer_elem_next(elem)) {
		if (is_capture(elem))
			cnt++;
	}

	return cnt;
}

static gp_widget *create_capture_widgets(snd_mixer_t *mixer)
{
	snd_mixer_elem_t *elem = snd_mixer_first_elem(mixer);
	unsigned int i = 0, count = capture_count(mixer);

	gp_widget *capture = gp_widget_grid_new(count, 3, 0);

	capture->align = GP_VFILL | GP_HCENTER;
	//TODO we probably need an API for this
	capture->grid->row_s[0].fill = 1;
	capture->grid->row_s[1].fill = 0;
	capture->grid->row_s[2].fill = 0;

	for (; elem != NULL; elem = snd_mixer_elem_next(elem)) {
		if (!is_capture(elem))
			continue;

		gp_widget *slider = create_capture_slider(elem);

		if (slider)
			gp_widget_grid_put(capture, i, 0, slider);

		gp_widget *chbox = create_capture_chbox(elem);
		gp_widget_grid_put(capture, i, 1, chbox);

		gp_widget *label = create_label(elem);
		gp_widget_grid_put(capture, i, 2, label);

		i++;
	}

	return capture;
}

static snd_mixer_t *do_mixer_init(uint8_t id)
{
	snd_mixer_t *mixer;

	struct snd_mixer_selem_regopt selem_regopt = {
		.ver = 1,
		.abstract = SND_MIXER_SABSTRACT_NONE,
		.device = "default",
	};

	if (snd_mixer_open(&mixer, id) < 0)
		return NULL;

	if (snd_mixer_selem_register(mixer, &selem_regopt, NULL) < 0)
		return NULL;

	snd_mixer_load(mixer);

	return mixer;
}

static int mixer_poll_callback(struct gp_fd *self, struct pollfd *pfd)
{
	(void) pfd;

	snd_mixer_handle_events(self->priv);

	return 0;
}

static void init_poll(snd_mixer_t *mixer)
{
	int i, nfds = snd_mixer_poll_descriptors_count(mixer);
	struct pollfd pfds[nfds];

	if (snd_mixer_poll_descriptors(mixer, pfds, nfds) < 0) {
		GP_WARN("Can't get mixer poll descriptors");
		return;
	}

	GP_DEBUG(1, "Initializing poll for %i fds\n", nfds);

	for (i = 0; i < nfds; i++) {
		gp_fds_add(gp_widgets_fds, pfds[i].fd, pfds[i].events, mixer_poll_callback, mixer);
	}
}

int main(int argc, char *argv[])
{
	snd_mixer_t *mixer = do_mixer_init(0);

	if (mixer == NULL)
		exit(0);

	init_poll(mixer);

	const char *tab_labels[] = {"Playback", "Capture"};

	gp_widget *layout = gp_widget_grid_new(1, 1, 0);
	gp_widget *tabs = gp_widget_tabs_new(2, 0, tab_labels, 0);
	gp_widget *playback = create_playback_widgets(mixer);
	gp_widget *capture = create_capture_widgets(mixer);

	gp_widget_grid_put(layout, 0, 0, tabs);
	gp_widget_tabs_put(tabs, 0, playback);
	gp_widget_tabs_put(tabs, 1, capture);

	gp_widgets_main_loop(layout, "alsa mixer", NULL, argc, argv);

	return 0;
}
