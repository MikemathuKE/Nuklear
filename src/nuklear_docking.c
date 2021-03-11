#include "nuklear.h"
#include "nuklear_internal.h"

NK_API void
nk_dockspace_init(struct nk_context *ctx, int width, int height)
{
    ctx->display_size.x = width;
    ctx->display_size.y = height;

    ctx->dockable_space.x = 0;
    ctx->dockable_space.y = 0;
    ctx->dockable_space.w = width;
    ctx->dockable_space.h = height;

    ctx->dck.windows = 0;
    ctx->dck.max_windows = 5;
    ctx->dck.num_windows = 0;
    ctx->dck.flags = 0;
    ctx->dck.next = 0;

    ctx->dock_breadth = 0;
    ctx->dock_depth = 0;
}

NK_API void
nk_dockspace_add_window(struct nk_context *ctx, struct nk_window* win)
{
    NK_ASSERT(ctx);
    NK_ASSERT(win);

    const struct nk_vec2 mouse_pos = ctx->input.mouse.pos;
    struct nk_rect top_bound = nk_dockspace_dock_highlight(ctx, NK_DOCK_HEADER);
    struct nk_rect left_bound = nk_dockspace_dock_highlight(ctx, NK_DOCK_LEFT);
    struct nk_dockspace *dck = &ctx->dck;

    if ((mouse_pos.x > top_bound.x) && (mouse_pos.x < top_bound.x + top_bound.w) &&
        (mouse_pos.y > top_bound.y) && (mouse_pos.y < top_bound.y + top_bound.h)) {
        nk_dockspace_remove_window(ctx, win);
        nk_dockspace_set(ctx, win, NK_DOCK_HEADER);
    } else if ((mouse_pos.x > left_bound.x) && (mouse_pos.x < left_bound.x + left_bound.w) &&
        (mouse_pos.y > left_bound.y) && (mouse_pos.y < left_bound.y + left_bound.h)) {
        nk_dockspace_remove_window(ctx, win);
        nk_dockspace_set(ctx, win, NK_DOCK_LEFT);
    } else {
        struct nk_dockspace *parent = nk_dockspace_find_window(ctx, win);
        struct nk_rect inner_bound = nk_dockspace_dock_inner_highlight(ctx);
        if (inner_bound.w != 0 && inner_bound.h != 0) {
            while ( inner_bound.x < dck->bounds.x || inner_bound.x >= dck->bounds.x + dck->bounds.w
                   || inner_bound.y < dck->bounds.y || inner_bound.y >= dck->bounds.y + dck->bounds.h) {
                dck = dck->next;
                if (dck == 0)
                    break;
            }
            if (dck == parent || dck == 0) {
                nk_dockspace_assign_window(ctx, parent, win, inner_bound);
                return;
            }
            nk_dockspace_append_window(ctx, dck, win, inner_bound);
            int i = 0;
            struct nk_dockspace *temp = &ctx->dck;
            while (temp != 0) {
                i++;
                temp = temp->next;
            }
            nk_dockspace_adjust(ctx, ctx->display_size.x, ctx->display_size.y);
        } else if(parent != 0) {
            nk_dockspace_assign_window(ctx, parent, win, inner_bound);
        }
        return;
    }
}

NK_API void
nk_dockspace_remove_window(struct nk_context *ctx, struct nk_window* win)
{
    NK_ASSERT(ctx);
    NK_ASSERT(win);

    struct nk_dockspace *dock_parent = &ctx->dck;
    if (dock_parent->windows == 0)
        return;
    struct nk_dockspace *dock_child = dock_parent;

    while (dock_child != 0) {
        if (dock_child->windows == 0)
            return;
        if (dock_child->num_windows == 1) {
            if (dock_child->windows->win == win) {
                win->bounds.w = dock_child->windows->originalWidth;
                win->bounds.h = dock_child->windows->originalHeight;
                if (dock_parent == dock_child) {
                    struct nk_dockspace *temp = ctx->dck.next;
                    ctx->dck = *(ctx->dck.next);

                    if (dock_child->flags & NK_DOCK_HEADER) {
                        ctx->dock_depth--;
                    }else if (dock_child->flags & NK_DOCK_LEFT) {
                        ctx->dock_breadth--;
                    }
                    dock_child = 0;
                    dock_parent = 0;
                    if(ctx->dck.num_windows != 0)
                        free(ctx->dck.next->windows);
                    free(temp);
                } else {
                    dock_parent->next = dock_child->next;
                    free(dock_child->windows);
                    free(dock_child);
                }
                return;
            }
        } else {
            for (int i = 0; i < dock_child->num_windows; i++) {
                if (dock_child->windows[i].win == win) {
                    win->bounds.w = dock_child->windows[i].originalWidth;
                    win->bounds.h = dock_child->windows[i].originalHeight;
                    for (int j = i; j < dock_child->num_windows; j++) {
                        dock_child->windows[j] = dock_child->windows[j+1];
                    }
                    dock_child->num_windows--;
                }
            }
        }
        if (dock_parent == dock_child) {
            dock_child = dock_parent->next;
        } else {
            dock_parent = dock_child;
            dock_child = dock_parent->next;
        }
    }
}


NK_API void
nk_dockspace_adjust(struct nk_context *ctx, int width, int height)
{
    NK_ASSERT(ctx);

    ctx->display_size.x = width;
    ctx->display_size.y = height;

    ctx->dockable_space.x = 0.0f;
    ctx->dockable_space.y = 0.0f;
    ctx->dockable_space.w = width;
    ctx->dockable_space.h = height;

    int depth = ctx->dock_depth;
    int breadth = ctx->dock_breadth;
    struct nk_vec2 min_size = ctx->style.window.min_size;

    struct nk_dockspace *dck = &ctx->dck;
    while (dck->num_windows != 0 ) {
        int win_count = 0;
        float w = 0.0f;
        float h = 0.0f;

        for (int i = 0; i < dck->num_windows; i++) {
            const struct nk_rect dockable = ctx->dockable_space;
            if ( dck->flags & NK_DOCK_HEADER ) {
                if (win_count == 0) {
                    dck->bounds.x = dockable.x;
                    dck->bounds.y = dockable.y;
                    dck->bounds.w = dockable.w;
                    dck->bounds.h = dck->windows[0].win->bounds.h;
                    dck->bounds.h = dck->bounds.h > ctx->display_size.y - depth * min_size.y ?
                        ctx->display_size.y - depth * min_size.y : dck->bounds.h;
                    ctx->dockable_space.y += dck->bounds.h;
                    ctx->dockable_space.h -= dck->bounds.h;
                    depth--;
                }
                win_count++;
                dck->windows[i].win->bounds.x = dck->bounds.x + w;
                dck->windows[i].win->bounds.y = dck->bounds.y;
                dck->windows[i].win->bounds.h = dck->bounds.h;
                if (i == dck->num_windows - 1) {
                    dck->windows[i].win->bounds.w = dck->bounds.w - w;
                    break;
                } else {
                    dck->windows[i].win->bounds.w = dck->windows[i].win->bounds.w < min_size.x ?
                    min_size.x : dck->windows[i].win->bounds.w;
                    dck->windows[i].win->bounds.w = dck->windows[i].win->bounds.x + dck->windows[i].win->bounds.w >
                        dck->bounds.x + dck->bounds.w - min_size.x * (dck->num_windows - win_count) ?
                        dck->bounds.w - min_size.x * (dck->num_windows - win_count) - dck->windows[i].win->bounds.x :
                        dck->windows[i].win->bounds.w;
                }
                w += dck->windows[i].win->bounds.w;
            } else if ( dck->flags & NK_DOCK_LEFT ) {
                if (win_count == 0) {
                    dck->bounds.x = dockable.x;
                    dck->bounds.y = dockable.y;
                    dck->bounds.h = dockable.h;
                    dck->bounds.w = dck->windows[i].win->bounds.w;
                    dck->bounds.w = dck->bounds.w > ctx->display_size.x - breadth * min_size.x ?
                        ctx->display_size.x - breadth * min_size.x : dck->bounds.w;
                    ctx->dockable_space.x += dck->bounds.w;
                    ctx->dockable_space.w -= dck->bounds.w;
                    breadth--;
                }
                win_count++;
                dck->windows[i].win->bounds.x = dck->bounds.x;
                dck->windows[i].win->bounds.y = dck->bounds.y + h;
                dck->windows[i].win->bounds.w = dck->bounds.w;
                if (i == dck->num_windows - 1) {
                    dck->windows[i].win->bounds.h = dck->bounds.h - h;
                    break;
                } else {
                    dck->windows[i].win->bounds.h = dck->windows[i].win->bounds.h < min_size.y ?
                    min_size.y : dck->windows[i].win->bounds.h;
                    dck->windows[i].win->bounds.h = dck->windows[i].win->bounds.y + dck->windows[i].win->bounds.h >
                        dck->bounds.y + dck->bounds.h - min_size.y * (dck->num_windows - win_count) ?
                        dck->bounds.h - min_size.y * (dck->num_windows - win_count) - dck->windows[i].win->bounds.y :
                        dck->windows[i].win->bounds.h;
                }
                h += dck->windows[i].win->bounds.h;
            }
        }
        dck = dck->next;
        if (dck == 0)
            return;
    }
}

NK_API struct nk_rect
nk_dockspace_dock_inner_highlight(struct nk_context *ctx)
{
    NK_ASSERT(ctx);

    const struct nk_vec2 mouse_pos = ctx->input.mouse.pos;
    struct nk_dockspace *dck = &ctx->dck;
    struct nk_rect inner_bound = {0, 0, 0, 0};

    while (dck != 0) {
        for (int i = 0; i < dck->num_windows; i++) {
            if (dck->windows[i].win == ctx->current) {
                continue;
            }

            struct nk_rect bounds = dck->windows[i].win->bounds;
            if (mouse_pos.x > bounds.x && mouse_pos.x < bounds.x + bounds.w
                && mouse_pos.y > bounds.y && mouse_pos.y < bounds.y + bounds.h) {
                if (dck->flags & NK_DOCK_HEADER) {
                    if (mouse_pos.x >= bounds.x && mouse_pos.x < bounds.x + bounds.w/2) {
                        inner_bound.x = bounds.x;
                        inner_bound.y = bounds.y;
                        inner_bound.w = bounds.w/2;
                        inner_bound.h = bounds.h;
                    } else if (mouse_pos.x >= bounds.x + bounds.w/2 && mouse_pos.x < bounds.x + bounds.w) {
                        inner_bound.x = bounds.x + bounds.w/2;
                        inner_bound.y = bounds.y;
                        inner_bound.w = bounds.w/2;
                        inner_bound.h = bounds.h;
                    }
                } else if (dck->flags & NK_DOCK_LEFT) {
                    if (mouse_pos.y >= bounds.y && mouse_pos.y < bounds.y + bounds.h/2) {
                        inner_bound.x = bounds.x;
                        inner_bound.y = bounds.y;
                        inner_bound.w = bounds.w;
                        inner_bound.h = bounds.h/2;
                    } else if (mouse_pos.y >= bounds.y + bounds.h/2 && mouse_pos.y < bounds.y + bounds.h) {
                        inner_bound.x = bounds.x;
                        inner_bound.y = bounds.y + bounds.h/2;
                        inner_bound.w = bounds.w;
                        inner_bound.h = bounds.h/2;
                    }
                }
                return inner_bound;
            }
        }
        dck = dck->next;
    }
    return inner_bound;
}

NK_API struct nk_rect
nk_dockspace_dock_highlight(const struct nk_context *ctx, nk_flags flag)
{
    NK_ASSERT(ctx);

    const struct nk_rect dockable = ctx->dockable_space;
    float set_width = dockable.w / 3;
    float set_height = dockable.h / 3;

    struct nk_rect bound;
    if (flag == NK_DOCK_HEADER) {
        bound.x = dockable.x + (dockable.w / 2) - (set_width / 2);
        bound.y = dockable.y;
        bound.w = set_width;
        bound.h = set_height;
    } else if (flag == NK_DOCK_LEFT) {
        bound.x = dockable.x;
        bound.y = dockable.y + (dockable.h / 2) - (set_height / 2);
        bound.w = set_width;
        bound.h = set_height;
    }

    return bound;
}

NK_API void
nk_dockspace_set(struct nk_context *ctx, struct nk_window *win, nk_flags flag)
{
    NK_ASSERT(ctx);
    NK_ASSERT(win);
    if (win == 0)
        return;
    struct nk_dockspace *dck = &ctx->dck;
    while (dck->num_windows != 0) {
        dck = dck->next;
    }

    const struct nk_rect dockable = ctx->dockable_space;
    if (dck->windows == 0) {
        dck->max_windows = 5;
        dck->windows = (struct nk_docked_window*)malloc(dck->max_windows * sizeof(struct nk_docked_window));
        dck->windows[0].win = win;
        dck->windows[0].originalWidth = win->bounds.w;
        dck->windows[0].originalHeight = win->bounds.h;
    } else {
        return;
    }

    if ( flag & NK_DOCK_HEADER ) {

        dck->flags = NK_DOCK_HEADER;

        win->bounds.x = dockable.x;
        win->bounds.y = dockable.y;
        win->bounds.w = dockable.w;
        win->bounds.h = win->bounds.h > ctx->dockable_space.h ? ctx->dockable_space.h : win->bounds.h;
        dck->bounds = win->bounds;

        ctx->dockable_space.y += win->bounds.h;
        ctx->dockable_space.h -= win->bounds.h;
        ctx->dock_depth++;
    } else if ( flag & NK_DOCK_LEFT ) {

        dck->flags = NK_DOCK_LEFT;

        win->bounds.x = dockable.x;
        win->bounds.y = dockable.y;
        win->bounds.h = dockable.h;
        win->bounds.w = win->bounds.w > ctx->dockable_space.w ? ctx->dockable_space.w : win->bounds.w;
        dck->bounds = win->bounds;

        ctx->dockable_space.x += win->bounds.w;
        ctx->dockable_space.w -= win->bounds.w;
        ctx->dock_breadth++;
    }
    dck->num_windows++;
    dck->next = (struct nk_dockspace*)malloc(sizeof(struct nk_dockspace));
    dck->next->windows = 0;
    dck->next->num_windows = 0;
    dck->next->flags = 0;
    dck->next->next = 0;
    nk_dockspace_adjust(ctx, ctx->display_size.x, ctx->display_size.y);
}

NK_API void
nk_dockspace_append_window(struct nk_context *ctx, struct nk_dockspace *dck, struct nk_window *win, struct nk_rect inner_bound)
{
    NK_ASSERT(ctx);
    NK_ASSERT(win);

    if (!win)
        return;

    nk_dockspace_remove_window(ctx, win);
    if(!dck || (dck->flags & NK_DOCK_LOCK)) {
        win->bounds.x = win->bounds.x < 0 ? 0 : win->bounds.x;
        win->bounds.y = win->bounds.y < 0 ? 0 : win->bounds.y;
        nk_dockspace_adjust(ctx, ctx->display_size.x, ctx->display_size.y);
        return;
    }

    if (dck->num_windows == dck->max_windows) {
        dck->max_windows += 5;
        dck->windows = (struct nk_docked_window*)realloc(dck->windows, dck->max_windows * sizeof(struct nk_docked_window));
    }

    for (int i = 0; i < dck->num_windows; i++) {
        if (dck->flags & NK_DOCK_HEADER) {
            if ( inner_bound.x == dck->windows[i].win->bounds.x) {
                struct nk_docked_window newWindow;
                newWindow.win = win;
                newWindow.originalWidth = win->bounds.w;
                newWindow.originalHeight = win->bounds.h;

                win->bounds.x = dck->windows[i].win->bounds.x;
                win->bounds.y = dck->windows[i].win->bounds.y;
                win->bounds.h = dck->windows[i].win->bounds.h;

                dck->windows[i].win->bounds.x += win->bounds.w;
                dck->windows[i].win->bounds.w -= win->bounds.w;

                for (int j = dck->num_windows; j > i; j--) {
                    struct nk_docked_window temp = dck->windows[j-1];
                    dck->windows[j] = temp;
                }
                dck->windows[i] = newWindow;
                dck->num_windows++;
                return;
            } else if ( inner_bound.x == dck->windows[i].win->bounds.x + dck->windows[i].win->bounds.w / 2) {
                struct nk_docked_window newWindow;
                newWindow.win = win;
                newWindow.originalWidth = win->bounds.w;
                newWindow.originalHeight = win->bounds.h;

                win->bounds.x = dck->windows[i].win->bounds.x + dck->windows[i].win->bounds.w - win->bounds.w;
                win->bounds.y = dck->windows[i].win->bounds.y;
                win->bounds.h = dck->windows[i].win->bounds.h;

                dck->windows[i].win->bounds.w -= win->bounds.w;
                i += 1;
                for (int j = dck->num_windows; j > i; j--) {
                    struct nk_docked_window temp = dck->windows[j-1];
                    dck->windows[j] = temp;
                }
                dck->windows[i] = newWindow;
                dck->num_windows++;
                return;
            }
        } else if (dck->flags & NK_DOCK_LEFT) {
            if ( inner_bound.y == dck->windows[i].win->bounds.y) {
                struct nk_docked_window newWindow;
                newWindow.win = win;
                newWindow.originalWidth = win->bounds.w;
                newWindow.originalHeight = win->bounds.h;

                win->bounds.x = dck->windows[i].win->bounds.x;
                win->bounds.y = dck->windows[i].win->bounds.y;
                win->bounds.w = dck->windows[i].win->bounds.w;
                dck->windows[i].win->bounds.y += win->bounds.h;
                dck->windows[i].win->bounds.y -= win->bounds.h;

                for (int j = dck->num_windows; j > i; j--) {
                    dck->windows[j] = dck->windows[j-1];
                }
                dck->windows[i] = newWindow;
                dck->num_windows++;
                return;
            } else if ( inner_bound.y == dck->windows[i].win->bounds.y + dck->windows[i].win->bounds.h / 2) {
                struct nk_docked_window newWindow;
                newWindow.win = win;
                newWindow.originalWidth = win->bounds.w;
                newWindow.originalHeight = win->bounds.h;

                win->bounds.x = dck->windows[i].win->bounds.x;
                win->bounds.y = dck->windows[i].win->bounds.y + dck->windows[i].win->bounds.h - win->bounds.h;
                win->bounds.w = dck->windows[i].win->bounds.w;

                dck->windows[i].win->bounds.h -= win->bounds.h;
                i += 1;
                for (int j = dck->num_windows; j > i; j--) {
                    dck->windows[j] = dck->windows[j-1];
                }
                dck->windows[i] = newWindow;
                dck->num_windows++;
                return;
            }
        }
    }
}

NK_API void
nk_dockspace_assign_window(struct nk_context *ctx, struct nk_dockspace *dck, struct nk_window *win, struct nk_rect inner_bound)
{
    if (dck == 0)
        return;

    const struct nk_vec2 mouse_pos = ctx->input.mouse.pos;
    if(mouse_pos.x > dck->bounds.x && mouse_pos.x < dck->bounds.x + dck->bounds.w
       && mouse_pos.y > dck->bounds.y && mouse_pos.y < dck->bounds.y + dck->bounds.h) {
        float width = 0.0f;
        float height = 0.0f;
        nk_bool swap_pos = nk_false;
        if (inner_bound.w != 0 && inner_bound.h != 0)
            swap_pos = nk_true;

        int i = 0;
        for (; i < dck->num_windows; i++) {
            if (dck->windows[i].win == win)
                break;
            if (dck->flags & NK_DOCK_HEADER) {
                width += dck->windows[i].win->bounds.w;
            } else if (dck->flags & NK_DOCK_LEFT) {
                height += dck->windows[i].win->bounds.h;
            }
        }
        if (!swap_pos) {
            win->bounds.x = dck->bounds.x + width;
            win->bounds.y = dck->bounds.y + height;
            return;
        }

        int j = 0;
        for (; j < dck->num_windows; j++) {
            if (dck->flags & NK_DOCK_HEADER) {
                if (inner_bound.x >= dck->windows[j].win->bounds.x &&
                inner_bound.x <= dck->windows[j].win->bounds.x + dck->windows[j].win->bounds.w)
                    break;
            } else if (dck->flags & NK_DOCK_LEFT) {
                if (inner_bound.y >= dck->windows[j].win->bounds.y &&
                inner_bound.y <= dck->windows[j].win->bounds.y + dck->windows[j].win->bounds.h)
                    break;
            }
        }
        struct nk_docked_window temp = dck->windows[j];
        dck->windows[j].win = dck->windows[i].win;
        dck->windows[j].originalWidth = dck->windows[i].originalWidth;
        dck->windows[j].originalHeight = dck->windows[i].originalHeight;

        dck->windows[i].win = temp.win;
        dck->windows[i].originalWidth = temp.originalWidth;
        dck->windows[i].originalHeight = temp.originalHeight;
    } else {
        nk_dockspace_remove_window(ctx, win);
        win->bounds.x = win->bounds.x < 0 ? 0 : win->bounds.x;
        win->bounds.y = win->bounds.y < 0 ? 0 : win->bounds.y;
    }
    nk_dockspace_adjust(ctx, ctx->display_size.x, ctx->display_size.y);
}

NK_API struct nk_dockspace*
nk_dockspace_find_window(struct nk_context *ctx, struct nk_window *win)
{
    NK_ASSERT(ctx);
    NK_ASSERT(win);

    struct nk_dockspace *dck = &ctx->dck;
    while (dck != 0) {
        for (int i = 0; i < dck->num_windows; i++) {
            if (dck->windows[i].win == win) {
                return dck;
            }
        }
        dck = dck->next;
    }
    return 0;
}

NK_API void
nk_dockspace_scale_horizontally(struct nk_context *ctx, struct nk_window *win, float delta)
{
    struct nk_dockspace *dck = nk_dockspace_find_window(ctx, win);
    if(dck != 0) {
        if(dck->flags & NK_DOCK_LEFT) {
            if (dck->windows->win != win)
                dck->windows->win->bounds.w += delta;
        }
    }
    nk_dockspace_adjust(ctx, ctx->display_size.x, ctx->display_size.y);
}

NK_API void
nk_dockspace_scale_vertically(struct nk_context *ctx, struct nk_window *win, float delta)
{
    struct nk_dockspace *dck = nk_dockspace_find_window(ctx, win);
    if(dck != 0) {
        if(dck->flags & NK_DOCK_HEADER)
            if (dck->windows->win != win)
                dck->windows->win->bounds.h += delta;
    }
    nk_dockspace_adjust(ctx, ctx->display_size.x, ctx->display_size.y);
}

NK_API void
nk_dockspace_end(struct nk_context *ctx)
{

}
