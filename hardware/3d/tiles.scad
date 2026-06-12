// Agent Indicator — 方案 D「Tiles」磁吸模块系统
// 90×90×22 模块 × 4 + 370×60×26 Dock 供电轨(前缘 80LED@164.7mm 拾音条)
// 一块 Matrix 模块悬浮展示磁吸/pogo 结构

$fn = 64;

C_BODY   = [0.16, 0.17, 0.19];
C_PANEL  = [0.09, 0.09, 0.11];
C_OFF    = [0.13, 0.13, 0.15];
C_RING   = [0.55, 0.75, 1.0];
C_SCREEN = [0.05, 0.07, 0.12];
C_POGO   = [0.85, 0.7, 0.2];

TILE = 90;
TILE_H = 22;

// ---- 模块外框 ----
module tile_frame() {
    color(C_BODY) difference() {
        hull() for (x = [5, TILE - 5], y = [5, TILE - 5])
            translate([x, y, 0]) cylinder(h = TILE_H, r = 5);
        translate([6, 6, 2]) cube([TILE - 12, TILE - 12, TILE_H]);
    }
    // 边缘 pogo 触点(四边中点)
    for (a = [0:3]) {
        rotate([0, 0, a * 90]) translate([TILE / 2, 0, TILE_H / 2])
            translate([-TILE / 2, -TILE / 2, 0]) /* 回到边中点 */ ;
    }
    color(C_POGO) {
        for (p = [[TILE/2, 1.2], [TILE/2, TILE-1.2]])
            for (dx = [-6, -2, 2, 6])
                translate([p[0]+dx, p[1], TILE_H/2]) sphere(d = 2.2);
    }
}

// ---- Matrix 模块 ----
module tile_matrix(lit_ratio = 0.6) {
    tile_frame();
    cell = 64 / 8;
    translate([(TILE - 64) / 2, (TILE - 64) / 2, TILE_H - 4]) {
        color(C_PANEL) cube([64, 64, 1]);
        for (i = [0:7], j = [0:7]) {
            idx = j * 8 + i;
            lit = idx < 64 * lit_ratio;
            cat = floor(idx / 13) % 5;
            c = !lit ? C_OFF
              : cat == 0 ? [0.35, 0.35, 0.42]
              : cat == 1 ? [0.95, 0.55, 0.05]
              : cat == 2 ? [0.05, 0.6, 0.8]
              : cat == 3 ? [0.6, 0.1, 0.8]
              : [0.05, 0.75, 0.4];
            color(c) translate([i * cell + 1, j * cell + 1, 1])
                cube([cell - 2, cell - 2, 0.8]);
        }
    }
}

// ---- Halo 模块 ----
module tile_halo() {
    tile_frame();
    translate([TILE / 2, TILE / 2, TILE_H - 4]) {
        color(C_SCREEN) cylinder(h = 1.4, d = 84);
        color([0.1, 0.35, 0.7]) translate([0, 0, 1.4]) cylinder(h = 0.4, d = 60);
        color(C_RING) translate([0, 0, 1])
            difference() {
                cylinder(h = 2.2, d = 85);
                translate([0, 0, -1]) cylinder(h = 5, d = 75);
            }
    }
}

// ---- Bar 模块(竖直 20LED usage) ----
module tile_bar() {
    tile_frame();
    seg = 41.5 / 20;
    translate([TILE / 2 - 2.5, (TILE - 41.5) / 2, TILE_H - 4]) {
        color(C_PANEL) translate([-2, -2, 0]) cube([9, 45.5, 1]);
        for (k = [0:19]) {
            lit = k < 13;
            c = !lit ? C_OFF : [k / 20, 1 - k / 20, 0.08];
            color(c) translate([0, k * seg + 0.3, 1]) cube([5, seg - 0.6, 0.8]);
        }
    }
}

// ---- Dock 供电轨 ----
module dock() {
    color(C_BODY) hull() for (x = [6, 364], y = [6, 54])
        translate([x, y, 0]) cylinder(h = 26, r = 6);
    // 燕尾槽示意
    color(C_PANEL) translate([10, 22, 26 - 4]) cube([350, 16, 4.1]);
    // 前缘拾音条 80LED@164.7
    n = 80; len = 164.7; seg = len / n;
    translate([(370 - len) / 2, -0.01, 9]) rotate([90, 0, 0]) {
        color(C_PANEL) translate([-2, -1.5, 0]) cube([len + 4, 8, 1]);
        for (k = [0:n - 1]) {
            dist = abs(k - (n - 1) / 2) / (n / 2);
            lit = dist < 0.4;
            c = !lit ? C_OFF : [0.05, 0.5 + 0.4 * (1 - dist), 0.85];
            color(c) translate([k * seg + 0.2, 0.5, 1]) cube([seg - 0.4, 4, 0.8]);
        }
    }
}

// ---- 场景 ----
dock();
translate([14, -16, 26]) rotate([12, 0, 0]) tile_halo();
translate([108, -16, 26]) rotate([12, 0, 0]) tile_matrix();
translate([202, -16, 26]) rotate([12, 0, 0]) tile_bar();
// 悬浮模块展示磁吸面
translate([296, 10, 78]) rotate([35, 8, -12]) tile_matrix(0.3);
