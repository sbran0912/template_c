#include "vehicle.h"

#include <math.h>
#include <stdlib.h>

/* ====================================================================
 * VEHICLE
 * ==================================================================== */

Vehicle vehicle_init(Body b) {
    Vehicle v = {0};
    v.body    = b;
    v.vel     = vec3(0, 0, 0);
    v.accel   = vec3(0, 0, 0);
    v.heading = vec3(0, 0, 0);
    v.health  = 1.0;
    v.dna[0]  = l3d_random_float(-1.0, 1.0);  // Kraft Richtung Gift
    v.dna[1]  = l3d_random_float(-1.0, 1.0);  // Kraft Richtung gutes Futter
    v.dna[2]  = l3d_random_float(20.0, 60.0); // Radius fuer Gift
    v.dna[3]  = l3d_random_float(20.0, 60.0); // Radius fuer gutes Futter
    return v;
}

void vehicle_align_to_velocity(Vehicle *v) {
    const double mag = vec3_length(v->vel);
    if (mag < 0.0001) {
        return;
    }

    const double magXZ = sqrt(v->vel.x * v->vel.x + v->vel.z * v->vel.z);

    // vel.y/mag muss in [-1,1] liegen (Schutz vor NaN durch Float-Rundung).
    const double cosY = l3d_constrain(v->vel.y / mag, -1.0, 1.0);
    v->body.rot.x = acos(cosY);
    if (magXZ < 0.0001) {
        v->body.rot.y = 0.0;
    } else {
        v->body.rot.y = atan2(v->vel.x / magXZ, v->vel.z / magXZ);
    }
    v->body.rot.z = 0.0;

    // Heading als normalisierte Richtung ableiten.
    v->heading = vec3_normalize(v->vel);
}

void vehicle_apply_force(Vehicle *v, Vec3 force) {
    v->accel = vec3_add(v->accel, force);
}

void vehicle_update(Vehicle *v) {
    v->vel = vec3_add(v->vel, v->accel);
    const double speed = vec3_length(v->vel);
    v->vel = vec3_scale(vec3_normalize(v->vel), l3d_constrain(speed, 0.5, 2.0));
    v->accel = vec3(0, 0, 0);
    v->body.pos = vec3_add(v->body.pos, v->vel);
}

// seekForce bestimmt, wie stark das Seek-Steering pro Frame in die
// Beschleunigung eingeht. Kleiner = ruhigeres, traegeres Anfliegen; die
// Geschwindigkeit selbst bleibt davon unberuehrt.
#define SEEK_FORCE 0.1

// steerLimit ist die Obergrenze der Steuerkraft (Laenge des Steering-Vektors).
// Kleiner = sanftere, weniger scharfe Korrekturen bei grossem Kursfehler,
// waehrend kleine Korrekturen unveraendert bleiben.
#define STEER_LIMIT 1.0

void vehicle_seek(Vehicle *v, Vec3 target, int is_bad_food) {
    Vec3 desired = vec3_limit(vec3_sub(target, v->body.pos), 3.0);
    desired = is_bad_food ? vec3_scale(desired, v->dna[0])
                          : vec3_scale(desired, v->dna[1]);
    const Vec3 steer = vec3_limit(vec3_sub(desired, v->vel), STEER_LIMIT);
    vehicle_apply_force(v, vec3_scale(steer, SEEK_FORCE));
}

void vehicle_eat_food(Vehicle *v, Food *food, int is_bad_food) {
    const double filter = is_bad_food ? v->dna[2] : v->dna[3];
    double mindist = INFINITY;
    int idx = -1;

    const size_t n = arr_len(*food);
    for (size_t i = 0; i < n; i++) {
        const double distance = vec3_distance(v->body.pos, (*food)[i]);
        if (distance < filter && distance < mindist) {
            mindist = distance;
            idx = (int) i;
        }
    }

    if (idx > -1) {
        if (vec3_distance(v->body.pos, (*food)[idx]) < 3.0) {
            // Food aufessen: aus dem Array entfernen genuegt - es gibt keinen
            // GPU-Buffer/Refcount mehr freizugeben (nur CPU-Geometrie).
            arr_pop(*food, (size_t) idx);
            v->health += is_bad_food ? -0.1 : 0.1;
        } else {
            vehicle_seek(v, (*food)[idx], is_bad_food);
        }
    }
}

void vehicle_boundary(Vehicle *v) {
    const double minX = -130.0;
    const double maxX = 130.0;
    const double minY = -130.0;
    const double maxY = 130.0;
    const double minZ = -130.0;
    const double maxZ = 130.0;

    if (v->body.pos.x < minX || v->body.pos.x > maxX) {
        v->vel.x *= -1.0;
    }
    if (v->body.pos.y < minY || v->body.pos.y > maxY) {
        v->vel.y *= -1.0;
    }
    if (v->body.pos.z < minZ || v->body.pos.z > maxZ) {
        v->vel.z *= -1.0;
    }
}

int vehicle_is_dead(const Vehicle *v) { return v->health < 0; }

/* ====================================================================
 * FUTTER
 * ==================================================================== */

Food new_food(int count) {
    Food food = arr_create((size_t) count, sizeof(Vec3));
    for (int i = 0; i < count; i++) {
        Vec3 p = random_food_pos();
        food = arr_push(food, &p);
    }
    return food;
}

void respawn(Food *food, int min, int count) {
    if ((int) arr_len(*food) < min) {
        for (int i = 0; i < count; i++) {
            Vec3 p = random_food_pos();
            *food = arr_push(*food, &p);
        }
    }
}

/* ====================================================================
 * HILFSFUNKTIONEN
 * ==================================================================== */

Mesh center_mesh_y(const Mesh *mesh, double dy) {
    const float d = (float) dy;
    Mesh out = {0};
    out.tri_floats  = mesh->tri_floats;
    out.line_floats = mesh->line_floats;

    if (mesh->tri_floats > 0) {
        out.triangles = malloc((size_t) mesh->tri_floats * sizeof(float));
        for (int i = 0; i < mesh->tri_floats; i++) {
            out.triangles[i] = mesh->triangles[i];
        }
        for (int i = 1; i < out.tri_floats; i += 3) {
            out.triangles[i] += d;
        }
    }
    if (mesh->line_floats > 0) {
        out.lines = malloc((size_t) mesh->line_floats * sizeof(float));
        for (int i = 0; i < mesh->line_floats; i++) {
            out.lines[i] = mesh->lines[i];
        }
        for (int i = 1; i < out.line_floats; i += 3) {
            out.lines[i] += d;
        }
    }
    return out;
}

Vec3 random_food_pos(void) {
    return vec3((double) l3d_random(-100, 100),
                (double) l3d_random(-100, 100),
                (double) l3d_random(-100, 100));
}
