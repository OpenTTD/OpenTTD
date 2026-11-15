//! Human pointer motion: how a person's hand moves a mouse from one place to another.
//!
//! - **Duration** follows Fitts' law, `MT = a + b * log2(D / W + 1)`: farther and smaller targets
//!   take longer, with a little noise from one movement to the next.
//! - **Shape** is a minimum-jerk profile (`10t^3 - 15t^4 + 6t^5`: slow start, fast middle, slow
//!   arrival) along a gently curved path, a quadratic Bezier whose control point sits off the
//!   straight line by a random fraction of the distance, because hands rotate about the wrist and
//!   elbow and rarely travel straight.
//! - **Long moves overshoot** the target slightly and correct with a short second sub-movement,
//!   as recorded human pointing does.
//! - **Tremor** adds a pixel of jitter along the way, never at the end point.
//!
//! Everything is seeded, so a run with the same seed moves the same way.

/// A small, seeded, dependency-free random source (xorshift64*).
pub struct Rng(u64);

impl Rng {
    pub fn new(seed: u64) -> Self {
        Rng(seed.wrapping_mul(0x9E37_79B9_7F4A_7C15) | 1)
    }

    pub fn next_u64(&mut self) -> u64 {
        let mut x = self.0;
        x ^= x >> 12;
        x ^= x << 25;
        x ^= x >> 27;
        self.0 = x;
        x.wrapping_mul(0x2545_F491_4F6C_DD1D)
    }

    /// Uniform in [0, 1).
    pub fn unit(&mut self) -> f64 {
        (self.next_u64() >> 11) as f64 / (1u64 << 53) as f64
    }

    pub fn range(&mut self, lo: f64, hi: f64) -> f64 {
        lo + (hi - lo) * self.unit()
    }

    /// Approximately normal (sum of uniforms), mean 0, standard deviation `sd`.
    pub fn normal(&mut self, sd: f64) -> f64 {
        let s: f64 = (0..6).map(|_| self.unit()).sum();
        (s - 3.0) * sd * std::f64::consts::SQRT_2
    }
}

/// One step of a movement: where the pointer is and how long to wait before the next step.
#[derive(Debug, Clone, Copy, PartialEq)]
pub struct Sample {
    pub x: i32,
    pub y: i32,
    pub wait_ms: u64,
}

/// The constants of the motion model; defaults are in the range reported for adults with a mouse.
#[derive(Debug, Clone, Copy)]
pub struct Model {
    /// Fitts' intercept, seconds.
    pub fitts_a: f64,
    /// Fitts' slope, seconds per bit.
    pub fitts_b: f64,
    /// Pointer updates per second.
    pub rate_hz: f64,
    /// Standard deviation of the path's sideways bow, as a fraction of the distance.
    pub curvature: f64,
    /// Moves longer than this (pixels) overshoot and correct.
    pub overshoot_from: f64,
    /// Tremor amplitude in pixels.
    pub tremor: f64,
    /// Speed factor: 1.0 is an ordinary person, 2.0 twice as fast.
    pub speed: f64,
}

impl Default for Model {
    fn default() -> Self {
        Model { fitts_a: 0.15, fitts_b: 0.14, rate_hz: 60.0, curvature: 0.08, overshoot_from: 280.0, tremor: 0.8, speed: 1.0 }
    }
}

fn min_jerk(t: f64) -> f64 {
    let t = t.clamp(0.0, 1.0);
    t * t * t * (10.0 - 15.0 * t + 6.0 * t * t)
}

impl Model {
    /// Fitts' law movement time in seconds for a distance and a target width (pixels).
    pub fn duration(&self, distance: f64, width: f64, rng: &mut Rng) -> f64 {
        let bits = (distance / width.max(4.0) + 1.0).log2();
        let base = self.fitts_a + self.fitts_b * bits;
        (base * (1.0 + rng.normal(0.08))).max(0.06) / self.speed.max(0.1)
    }

    /// One sub-movement from `from` to `to`, minimum-jerk along a bowed path.
    fn submovement(&self, from: (f64, f64), to: (f64, f64), seconds: f64, rng: &mut Rng, tremor: bool) -> Vec<Sample> {
        let (dx, dy) = (to.0 - from.0, to.1 - from.1);
        let dist = (dx * dx + dy * dy).sqrt();
        // The control point: the midpoint pushed sideways by a random fraction of the distance.
        let bow = rng.normal(self.curvature) * dist;
        let (nx, ny) = if dist > 0.0 { (-dy / dist, dx / dist) } else { (0.0, 0.0) };
        let ctrl = (from.0 + dx * 0.5 + nx * bow, from.1 + dy * 0.5 + ny * bow);
        let steps = ((seconds * self.rate_hz).ceil() as usize).max(2);
        let wait = (seconds * 1000.0 / steps as f64).round().max(1.0) as u64;
        let mut out = Vec::with_capacity(steps);
        for i in 1..=steps {
            let s = min_jerk(i as f64 / steps as f64);
            let u = 1.0 - s;
            let mut x = u * u * from.0 + 2.0 * u * s * ctrl.0 + s * s * to.0;
            let mut y = u * u * from.1 + 2.0 * u * s * ctrl.1 + s * s * to.1;
            if tremor && i < steps {
                x += rng.normal(self.tremor);
                y += rng.normal(self.tremor);
            }
            out.push(Sample { x: x.round() as i32, y: y.round() as i32, wait_ms: wait });
        }
        out
    }

    /// The whole movement from `from` to a target of `width` pixels centred on `to`: one
    /// sub-movement, or an overshoot and a correction for a long move. The last sample is `to`.
    pub fn path(&self, from: (i32, i32), to: (i32, i32), width: f64, rng: &mut Rng) -> Vec<Sample> {
        let f = (from.0 as f64, from.1 as f64);
        let t = (to.0 as f64, to.1 as f64);
        let (dx, dy) = (t.0 - f.0, t.1 - f.1);
        let dist = (dx * dx + dy * dy).sqrt();
        if dist < 1.0 {
            return vec![Sample { x: to.0, y: to.1, wait_ms: 16 }];
        }
        let total = self.duration(dist, width, rng);
        let mut out = Vec::new();
        if dist > self.overshoot_from {
            // Overshoot along the direction of travel by 3-7%, a little off-axis, then correct.
            let k = rng.range(0.03, 0.07);
            let over = (t.0 + dx * k + rng.normal(width * 0.15), t.1 + dy * k + rng.normal(width * 0.15));
            out.extend(self.submovement(f, over, total * 0.82, rng, true));
            out.extend(self.submovement(over, t, total * 0.18 + 0.05, rng, false));
        } else {
            out.extend(self.submovement(f, t, total, rng, true));
        }
        if let Some(last) = out.last_mut() {
            last.x = to.0;
            last.y = to.1;
        }
        out
    }

    /// A deliberate drag at a steady, slower pace (a person dragging a selection watches it).
    pub fn drag(&self, from: (i32, i32), to: (i32, i32), rng: &mut Rng) -> Vec<Sample> {
        let (dx, dy) = ((to.0 - from.0) as f64, (to.1 - from.1) as f64);
        let dist = (dx * dx + dy * dy).sqrt();
        let seconds = (0.35 + dist / 900.0) * (1.0 + rng.normal(0.06)) / self.speed.max(0.1);
        let steps = ((seconds * self.rate_hz).ceil() as usize).max(2);
        let wait = (seconds * 1000.0 / steps as f64).round().max(1.0) as u64;
        (1..=steps)
            .map(|i| {
                let s = min_jerk(i as f64 / steps as f64);
                Sample { x: (from.0 as f64 + dx * s).round() as i32, y: (from.1 as f64 + dy * s).round() as i32, wait_ms: wait }
            })
            .collect()
    }

    /// How long a person pauses before acting (reading, deciding), in milliseconds.
    pub fn think_ms(&self, rng: &mut Rng, lo: f64, hi: f64) -> u64 {
        (rng.range(lo, hi) / self.speed.max(0.1)) as u64
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn a_path_ends_exactly_on_its_target() {
        let m = Model::default();
        let mut rng = Rng::new(7);
        for &(from, to) in &[((0, 0), (900, 400)), ((500, 500), (520, 505)), ((10, 10), (10, 10))] {
            let p = m.path(from, to, 40.0, &mut rng);
            let last = p.last().unwrap();
            assert_eq!((last.x, last.y), to);
        }
    }

    #[test]
    fn farther_and_smaller_targets_take_longer() {
        let m = Model { ..Model::default() };
        let mut rng = Rng::new(1);
        let near: f64 = (0..50).map(|_| m.duration(100.0, 40.0, &mut rng)).sum();
        let far: f64 = (0..50).map(|_| m.duration(1200.0, 40.0, &mut rng)).sum();
        let small: f64 = (0..50).map(|_| m.duration(1200.0, 8.0, &mut rng)).sum();
        assert!(near < far && far < small, "near {near} far {far} small {small}");
    }

    #[test]
    fn the_profile_is_slow_at_both_ends_and_fast_in_the_middle() {
        let m = Model { tremor: 0.0, curvature: 0.0, overshoot_from: 1e9, ..Model::default() };
        let mut rng = Rng::new(3);
        let p = m.path((0, 0), (1000, 0), 40.0, &mut rng);
        let steps: Vec<i32> = p.windows(2).map(|w| (w[1].x - w[0].x).abs()).collect();
        let mid = steps[steps.len() / 2];
        assert!(steps[0] < mid && steps[steps.len() - 1] < mid, "{steps:?}");
    }

    #[test]
    fn a_long_move_overshoots_then_corrects() {
        let m = Model { tremor: 0.0, curvature: 0.0, ..Model::default() };
        let mut rng = Rng::new(11);
        let p = m.path((0, 0), (1000, 0), 40.0, &mut rng);
        assert!(p.iter().any(|s| s.x > 1000), "no overshoot past the target");
        assert_eq!(p.last().map(|s| s.x), Some(1000));
    }

    #[test]
    fn the_same_seed_moves_the_same_way() {
        let m = Model::default();
        let a = m.path((0, 0), (700, 300), 30.0, &mut Rng::new(42));
        let b = m.path((0, 0), (700, 300), 30.0, &mut Rng::new(42));
        assert_eq!(a, b);
    }
}
