#include <random>

#include "libslic3r/Arachne/utils/ExtrusionJunction.hpp"
#include "libslic3r/Arachne/utils/ExtrusionLine.hpp"
#include "libslic3r/PerimeterGenerator.hpp"
#include "libslic3r/Point.hpp"
#include "libslic3r/Polygon.hpp"
#include "libslic3r/PrintConfig.hpp"

#include "FuzzySkin.hpp"

using namespace Slic3r;

namespace Slic3r::Feature::FuzzySkin {

// Produces a random value between 0 and 1. Thread-safe.
static double random_value()
{
    thread_local std::random_device rd;
    // Hash thread ID for random number seed if no hardware rng seed is available
    thread_local std::mt19937                           gen(rd.entropy() > 0 ? rd() : std::hash<std::thread::id>()(std::this_thread::get_id()));
    thread_local std::uniform_real_distribution<double> dist(0.0, 1.0);
    return dist(gen);
}

void fuzzy_polyline(Points &poly, const bool closed, const double fuzzy_skin_thickness, const double fuzzy_skin_point_distance)
{
    const double min_dist_between_points = fuzzy_skin_point_distance * 3. / 4.; // hardcoded: the point distance may vary between 3/4 and 5/4 the supplied value
    const double range_random_point_dist = fuzzy_skin_point_distance / 2.;
    double       dist_left_over          = random_value() * (min_dist_between_points / 2.); // the distance to be traversed on the line before making the first new point

    Points out;
    out.reserve(poly.size());

    // Skip the first point for open polyline.
    Point *p0 = closed ? &poly.back() : &poly.front();
    for (auto it_pt1 = closed ? poly.begin() : std::next(poly.begin()); it_pt1 != poly.end(); ++it_pt1) {
        Point &p1 = *it_pt1;

        // 'a' is the (next) new point between p0 and p1
        Vec2d  p0p1      = (p1 - *p0).cast<double>();
        double p0p1_size = p0p1.norm();
        double p0pa_dist = dist_left_over;
        for (; p0pa_dist < p0p1_size; p0pa_dist += min_dist_between_points + random_value() * range_random_point_dist) {
            double r = random_value() * (fuzzy_skin_thickness * 2.) - fuzzy_skin_thickness;
            out.emplace_back(*p0 + (p0p1 * (p0pa_dist / p0p1_size) + perp(p0p1).cast<double>().normalized() * r).cast<coord_t>());
        }

        dist_left_over = p0pa_dist - p0p1_size;
        p0             = &p1;
    }

    while (out.size() < 3) {
        size_t point_idx = poly.size() - 2;
        out.emplace_back(poly[point_idx]);
        if (point_idx == 0) {
            break;
        }

        --point_idx;
    }

    if (out.size() >= 3) {
        poly = std::move(out);
    }
}

void fuzzy_polygon(Polygon &polygon, double fuzzy_skin_thickness, double fuzzy_skin_point_distance)
{
    fuzzy_polyline(polygon.points, true, fuzzy_skin_thickness, fuzzy_skin_point_distance);
}

void fuzzy_extrusion_line(Arachne::ExtrusionLine &ext_lines, const double fuzzy_skin_thickness, const double fuzzy_skin_point_distance)
{
    const double min_dist_between_points = fuzzy_skin_point_distance * 3. / 4.; // hardcoded: the point distance may vary between 3/4 and 5/4 the supplied value
    const double range_random_point_dist = fuzzy_skin_point_distance / 2.;
    double       dist_left_over          = random_value() * (min_dist_between_points / 2.); // the distance to be traversed on the line before making the first new point

    Arachne::ExtrusionJunction *p0 = &ext_lines.front();
    Arachne::ExtrusionJunctions out;
    out.reserve(ext_lines.size());
    for (auto &p1 : ext_lines) {
        if (p0->p == p1.p) {
            // Copy the first point.
            out.emplace_back(p1.p, p1.w, p1.perimeter_index);
            continue;
        }

        // 'a' is the (next) new point between p0 and p1
        Vec2d  p0p1      = (p1.p - p0->p).cast<double>();
        double p0p1_size = p0p1.norm();
        double p0pa_dist = dist_left_over;
        for (; p0pa_dist < p0p1_size; p0pa_dist += min_dist_between_points + random_value() * range_random_point_dist) {
            double r = random_value() * (fuzzy_skin_thickness * 2.) - fuzzy_skin_thickness;
            out.emplace_back(p0->p + (p0p1 * (p0pa_dist / p0p1_size) + perp(p0p1).cast<double>().normalized() * r).cast<coord_t>(), p1.w, p1.perimeter_index);
        }

        dist_left_over = p0pa_dist - p0p1_size;
        p0             = &p1;
    }

    while (out.size() < 3) {
        size_t point_idx = ext_lines.size() - 2;
        out.emplace_back(ext_lines[point_idx].p, ext_lines[point_idx].w, ext_lines[point_idx].perimeter_index);
        if (point_idx == 0) {
            break;
        }

        --point_idx;
    }

    if (ext_lines.back().p == ext_lines.front().p) {
        // Connect endpoints.
        out.front().p = out.back().p;
    }

    if (out.size() >= 3) {
        ext_lines.junctions = std::move(out);
    }
}

bool should_fuzzify(const PrintRegionConfig &config, const size_t layer_idx, const size_t perimeter_idx, const bool is_contour)
{
    const FuzzySkinType fuzzy_skin_type = config.fuzzy_skin.value;

    if (fuzzy_skin_type == FuzzySkinType::None || layer_idx <= 0) {
        return false;
    }

    const bool fuzzify_contours = perimeter_idx == 0;
    const bool fuzzify_holes    = fuzzify_contours && fuzzy_skin_type == FuzzySkinType::All;

    return is_contour ? fuzzify_contours : fuzzify_holes;
}

} // namespace Slic3r::Feature::FuzzySkin
