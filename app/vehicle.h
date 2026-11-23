#ifndef VEHICLE_H
#define VEHICLE_H

#include "lib3d.h"
#include "lib3d_util.h"

/* ====================================================================
 * FUTTER
 *
 * Food ist eine Menge von Futterpositionen (dynamisches arr_*-Array von
 * Vec3). Das gemeinsame Mesh wird nicht hier verwaltet, daher reichen
 * die Positionen.
 * ==================================================================== */
typedef Vec3 *Food;

/* ====================================================================
 * VEHICLE - Fahrzeug mit Steering-Behavior (Seek) und DNA
 *
 * Ein Vehicle ist ein Body (Mesh + Position/Rotation) mit zusaetzlichem
 * Physik-Zustand (Geschwindigkeit/Beschleunigung), einer Gesundheit und
 * einer DNA. Die DNA steuert, wie stark Gift bzw. gutes Futter
 * angesteuert wird und in welchem Radius es ueberhaupt wahrgenommen wird.
 *
 * Definition: vehicle.c
 * ==================================================================== */
typedef struct {
    Body   body;
    Vec3   vel;
    Vec3   accel;
    Vec3   heading;
    double health;
    double dna[4];
} Vehicle;

/* vehicle_init erzeugt ein Fahrzeug aus einem Body mit zufaelliger DNA. */
Vehicle vehicle_init(Body b);

/* vehicle_align_to_velocity passt die Richtung des Bodys an die
 * Geschwindigkeit an. */
void vehicle_align_to_velocity(Vehicle *v);

/* vehicle_apply_force wendet eine Kraft an (akkumuliert in accel). */
void vehicle_apply_force(Vehicle *v, Vec3 force);

/* vehicle_update ist das Physik-Update: Geschwindigkeit aus Beschleunigung,
 * Position aus Geschwindigkeit. Die Geschwindigkeit wird auf [0.5, 2.0]
 * begrenzt. */
void vehicle_update(Vehicle *v);

/* vehicle_seek ist das Seek-Verhalten. Die gewuenschte Richtung wird je nach
 * Futter-Art mit dem passenden DNA-Wert skaliert (dna[0] fuer Gift, dna[1]
 * fuer gutes Futter). */
void vehicle_seek(Vehicle *v, Vec3 target, int is_bad_food);

/* vehicle_eat_food sucht das naechste Futter im DNA-Radius und frisst es,
 * wenn es nah genug ist. Gutes Futter -> Health +0.1, Gift -> Health -0.1.
 * Ansonsten wird es angesteuert (seek). Das Food-Array wird ggf. gekuerzt. */
void vehicle_eat_food(Vehicle *v, Food *food, int is_bad_food);

/* vehicle_boundary kehrt die Geschwindigkeit an den Raendern um (Bounce). */
void vehicle_boundary(Vehicle *v);

/* vehicle_is_dead gibt 1 zurueck, wenn das Vehicle tot ist (Health < 0). */
int vehicle_is_dead(const Vehicle *v);

/* ====================================================================
 * HILFSFUNKTIONEN
 * ==================================================================== */

/* new_food erzeugt count Futterpositionen an Zufallspositionen. */
Food new_food(int count);

/* respawn fuellt das Futter-Array auf: sind weniger als min vorhanden,
 * kommen count neue dazu. Der Array-Zeiger kann sich dabei verschieben. */
void respawn(Food *food, int min, int count);

/* center_mesh_y verschiebt eine Kopie des Meshes entlang Y.
 * make_pyramid_mesh legt die Basis bei y=0 (Spitze bei y=height); fuer ein
 * um die Position zentriertes Fahrzeug wird das Mesh um -height/2
 * verschoben. Das Eingabe-Mesh bleibt unveraendert (mesh_free() beim
 * Ergebnis). */
Mesh center_mesh_y(const Mesh *mesh, double dy);

/* random_food_pos liefert eine zufaellige Futterposition in [-100, 100). */
Vec3 random_food_pos(void);

#endif // VEHICLE_H
