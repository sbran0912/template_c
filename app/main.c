#include <stddef.h>
#include <stdio.h>

#include "lib3d.h"
#include "lib3d_util.h"
#include "vehicle.h"

// ====================================================================
// Vehicle - Fahrzeug-Schwarm mit Steering-Behaviors und DNA
//
// Zehn Fahrzeuge (Pyramiden) steuern per Seek gutes Futter (gruen) und Gift
// (rot) an; die DNA bestimmt Staerke und Wahrnehmungsradius. Fressen veraendert
// die Gesundheit, Altern (1.5 % pro Frame) zehrt sie auf; bei Health < 0
// stirbt ein Fahrzeug.
//
// Gegenueber einem einfachen Body-Ansatz:
//   - Szene/Kamera/Nebel als Buendel (Scene/l3d_begin_scene).
//   - GPU-Instancing: alle Futterkugeln in EINEM Draw-Call (Farbe pro
//     Instanz), alle Fahrzeuge in EINEM Draw-Call (Farbe nach Health).
//   - Expliziter l3d_flush_batch() zwischen opaker Welt und Debug-Overlays,
//     damit die transparenten DNA-Kreise weiterhin zuletzt (obenauf)
//     gezeichnet werden.
// ====================================================================

#define VEHICLE_COUNT 10

// --- Globale Szenen-/Loop-Daten ---
static Scene scene;
static Mesh  gridMesh;
static Mesh  foodMesh;
static Mesh  vehicleMesh;

static Food poison;
static Food food;

static Vehicle vehicles[VEHICLE_COUNT];
static int     vehicleCount = 0;

// Instanz-Puffer (arr_* von Instance).
static Instance *foodInstances    = NULL;
static Instance *vehicleInstances = NULL;

// Zeichen-Stile und Instanz-Farben.
static ShapeStyle gridStyle;
static ShapeStyle foodStyle;
static ShapeStyle vehicleStyle;

static ColorState poisonColor;
static ColorState foodColor;
static ColorState vehicleHealthy;
static ColorState vehicleWeak;

// Loop (Ersatz fuer die draw-Closure in C++).
static void draw(void) {
    l3d_begin_scene(scene);

    // Futter nachwachsen lassen (min 20, +30 pro Respawn).
    respawn(&food, 20, 30);
    respawn(&poison, 20, 30);

    // Altern: mit 1.5% Wahrscheinlichkeit pro Frame altern alle Fahrzeuge.
    const int getOlder = l3d_random_float(0, 1) < 0.015;

    // Fahrzeuge simulieren (rueckwaerts, damit Entfernen beim Iterieren ok
    // ist).
    for (int i = vehicleCount; i > 0; i--) {
        Vehicle *v = &vehicles[i - 1];

        vehicle_boundary(v);
        vehicle_eat_food(v, &food, 0);   // gutes Futter
        vehicle_eat_food(v, &poison, 1); // Gift
        vehicle_align_to_velocity(v);
        vehicle_update(v);

        if (getOlder) {
            v->health -= 0.05;
        }

        if (vehicle_is_dead(v)) {
            // Entfernen: nachfolgende Elemente nachruecken (wie vector::erase).
            for (int j = i - 1; j < vehicleCount - 1; j++) {
                vehicles[j] = vehicles[j + 1];
            }
            vehicleCount--;
        }
    }

    // Bodengitter (XZ-Ebene bei y=0) aus dem geteilten Mesh.
    l3d_draw_mesh(gridMesh, gridStyle, NULL);

    // Futter-Instanzen: Gift (rot) + gutes Futter (gruen) in einem Puffer.
    arr_delete(foodInstances);
    const size_t poisonLen = arr_len(poison);
    for (size_t i = 0; i < poisonLen; i++) {
        const Vec3 p = poison[i];
        Instance inst = { .model = mat4_translate(p.x, p.y, p.z),
                          .color = poisonColor };
        foodInstances = arr_push(foodInstances, &inst);
    }
    const size_t foodLen = arr_len(food);
    for (size_t i = 0; i < foodLen; i++) {
        const Vec3 f = food[i];
        Instance inst = { .model = mat4_translate(f.x, f.y, f.z),
                          .color = foodColor };
        foodInstances = arr_push(foodInstances, &inst);
    }

    // Fahrzeug-Instanzen: Farbe nach Gesundheit (orange wenn schwach).
    arr_delete(vehicleInstances);
    for (int i = 0; i < vehicleCount; i++) {
        const Vehicle *v = &vehicles[i];
        const ColorState col = v->health < 0.5 ? vehicleWeak : vehicleHealthy;
        Instance inst = { .model = body_model_matrix(&v->body), .color = col };
        vehicleInstances = arr_push(vehicleInstances, &inst);
    }

    // Je EIN Instancing-Draw-Call fuer Futter bzw. Fahrzeuge.
    l3d_draw_mesh_instanced(foodMesh, foodStyle, (const Instance *) foodInstances,
                            (int) arr_len(foodInstances));
    l3d_draw_mesh_instanced(vehicleMesh, vehicleStyle,
                            (const Instance *) vehicleInstances,
                            (int) arr_len(vehicleInstances));

    // Opake Welt zeichnen, BEVOR die transparenten Debug-Overlays kommen.
    l3d_flush_batch();

    // Debug-Overlays (Batched Drawing): Heading-Pfeile + DNA-Radien.
    for (int i = 0; i < vehicleCount; i++) {
        const Vehicle *v = &vehicles[i];
        const Vec3 hp = v->body.pos;

        // Heading-Pfeil: kurze Linie vom Fahrzeug in Fahrtrichtung (kuerzer
        // als der Koerper, damit beides nicht zu einer Linie verschmilzt).
        const Vec3 end = vec3_add(hp, vec3_scale(v->heading, 5));
        PrimitiveStyle arrowStyle = {0};
        arrowStyle.stroke = color_hex("#ffffff");
        if (v->health < 0.5) {
            arrowStyle.stroke = color_hex("#ff4444");
        }
        l3d_draw_line(hp.x, hp.y, hp.z, end.x, end.y, end.z, arrowStyle, NULL);

        // DNA-Radien als Kreise in der XZ-Ebene. l3d_draw_circle(x,y,z)
        // zeichnet in der XY-Ebene - durch den Tausch (x, z, y) liegt der
        // Kreis flach.
        ShapeStyle dnaGood = {0};
        dnaGood.stroke = color_rgba(51, 255, 51, 64);
        dnaGood.segments = 48;
        l3d_draw_circle(hp.x, hp.z, hp.y, v->dna[3], dnaGood, NULL);

        ShapeStyle dnaBad = {0};
        dnaBad.stroke = color_rgba(255, 51, 51, 64);
        dnaBad.segments = 48;
        l3d_draw_circle(hp.x, hp.z, hp.y, v->dna[2], dnaBad, NULL);
    }
}

int main(void) {
    scene = scene_new();
    scene.cam.pos    = vec3(50, 20, 200);
    scene.cam.target = vec3(0, 0, 0);
    scene.cam.up     = vec3(0, 1, 0);
    scene.cam.fovY   = 1.2;
    scene.cam.near   = 0.1;
    scene.cam.far    = 1000;
    scene.sunDir     = vec3(0.5, 1.0, 0.3);
    scene.fog.enabled = 1;
    scene.fog.near    = 100;
    scene.fog.far     = 400;
    scene.fog.color   = (ColorState){0.25, 0.25, 0.25, 1};

    // mode bleibt STYLE_STROKE (0) - nur die Linienfarbe wird gesetzt.
    gridStyle.stroke    = color_hex("#777774");
    gridStyle.lineWidth = 1;

    // Futter/Fahrzeuge: Farbe kommt beim Instancing pro Instance; der Style
    // bestimmt nur Fill+Stroke und Linienbreite.
    foodStyle.lineWidth = 1;
    foodStyle.mode      = STYLE_BOTH;
    vehicleStyle.lineWidth = 1;
    vehicleStyle.mode      = STYLE_BOTH;

    // Meshes EINMAL erzeugen und ueber alle Instanzen teilen.
    gridMesh = make_grid_mesh(600, 24);      // Bodengitter
    foodMesh = make_sphere_mesh(3, 16, 16);  // Futter-Kugel (Radius 3)

    // Fahrzeug-Pyramide: breit und kompakt (kein duenner Spiess), damit der
    // Koerper aus jeder Blickrichtung als 3D-Form lesbar ist. Basis bei y=0 ->
    // um die Position zentrieren.
    const double vehicleBase   = 6.0;
    const double vehicleHeight = 12.0;
    Mesh pyramid = make_pyramid_mesh(vehicleBase, vehicleHeight);
    vehicleMesh = center_mesh_y(&pyramid, -vehicleHeight / 2);
    mesh_free(&pyramid);

    // Instanz-Farben.
    poisonColor    = parse_hex("#FF0000");
    foodColor      = parse_hex("#44ff44");
    vehicleHealthy = parse_hex("#ffffff");
    // Schwache Fahrzeuge bewusst NICHT rot (sonst mit dem roten Gift-Futter
    // verwechselbar), sondern orange.
    vehicleWeak    = parse_hex("#ff8800");

    // 30x Gift (rot) und 30x gutes Futter (gruen).
    poison = new_food(30);
    food   = new_food(30);

    // 10 Fahrzeuge mit zufaelliger Startgeschwindigkeit.
    for (int i = 0; i < VEHICLE_COUNT; i++) {
        Body b = make_body(vehicleMesh, vec3(0, 20, 100), vec3(0, 0, 0));
        Vehicle v = vehicle_init(b);
        v.vel = vec3(l3d_random_float(-2, 2),
                     l3d_random_float(-2, 2),
                     l3d_random_float(-2, 2));
        vehicles[i] = v;
    }
    vehicleCount = VEHICLE_COUNT;

    // Wiederverwendete Instanz-Puffer.
    foodInstances    = arr_create(60, sizeof(Instance));
    vehicleInstances = arr_create(VEHICLE_COUNT, sizeof(Instance));

    /* Start */
    if (!l3d_init()) {
        fprintf(stderr, "vehicle: Initialisierung fehlgeschlagen\n");
        return 1;
    }

    l3d_start_animation(draw);

    // Aufraeumen (nach dem Schliessen des Fensters).
    for (int i = 0; i < vehicleCount; i++) {
        body_free(&vehicles[i].body);
    }
    arr_free(foodInstances);
    arr_free(vehicleInstances);
    arr_free(food);
    arr_free(poison);
    mesh_free(&gridMesh);
    mesh_free(&foodMesh);
    mesh_free(&vehicleMesh);

    return 0;
}
