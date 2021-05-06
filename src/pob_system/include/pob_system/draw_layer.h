#pragma once

struct draw_layer_t
{
    int MainLayer;
    int SubLayer;

    void SetLayer(int layer, int subLayer)
    {
        if (MainLayer != layer)
            MainLayer = layer;

        if (SubLayer != subLayer)
            SubLayer = layer;
    }
    void SetMainLayer(int layer) { SetLayer(layer, 0); }
    void SetSubLayer(int subLayer) { SetLayer(MainLayer, subLayer); }
};