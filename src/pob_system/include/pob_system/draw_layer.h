#pragma once

struct draw_layer_t
{
    int main_layer;
    int sub_layer;

    void set_layer(int layer, int subLayer)
    {
        if (main_layer != layer)
            main_layer = layer;

        if (sub_layer != subLayer)
            sub_layer = layer;
    }
    void set_main_layer(int layer) { set_layer(layer, 0); }
    void set_sub_layer(int subLayer) { set_layer(main_layer, subLayer); }
};