// Agent Indicator — 方案 A「Halo」圆形单体
// 本体 Ø100×32mm 后仰 10°,跑道形底座 110×70×30mm,前缘拾音条 48LED@99mm
// 与 docs/03 §方案 A 尺寸一致

$fn = 96;

C_BODY   = [0.16, 0.17, 0.19];
C_PANEL  = [0.09, 0.09, 0.11];
C_OFF    = [0.13, 0.13, 0.15];
C_RING   = [0.55, 0.75, 1.0];
C_SCREEN = [0.05, 0.07, 0.12];

TILT = 10;

// ---- 底座:圆角矩形 110×70×30(r=5,前缘直边 100mm 容纳拾音条) ----
module base() {
    color(C_BODY) hull()
        for (x = [-50, 50], y = [-30, 30])
            translate([x, y, 0]) cylinder(h = 30, r = 5);
}

// ---- 拾音条(48 LED @99mm)嵌底座前立面 ----
module vu_bar() {
    n = 48; len = 99; seg = len / n;
    translate([-len / 2, -35.01, 11]) rotate([90, 0, 0]) {
        color(C_PANEL) translate([-2, -1.5, 0]) cube([len + 4, 8, 1]);
        for (k = [0:n - 1]) {
            dist = abs(k - (n - 1) / 2) / (n / 2);
            lit = dist < 0.5;
            c = !lit ? C_OFF : [0.05, 0.5 + 0.4 * (1 - dist), 0.85];
            color(c) translate([k * seg + 0.25, 0, 1])
                cube([seg - 0.5, 5, 0.8]);
        }
    }
}

// ---- 本体:Ø100×32 圆柱,前面 Halo ----
module head() {
    translate([0, 12, 78]) rotate([90 - TILT, 0, 0]) {
        color(C_BODY) cylinder(h = 32, d = 100);          // 机身
        // 前面板(朝 -y 即朝观察者):放在 z=32 端
        translate([0, 0, 32]) {
            color(C_PANEL) cylinder(h = 0.8, d = 96);
            color(C_SCREEN) translate([0, 0, 0.8]) cylinder(h = 1.2, d = 84);
            color([0.1, 0.35, 0.7]) translate([0, 0, 2])
                cylinder(h = 0.4, d = 62);                 // 圆形 UI 亮区
            color(C_RING) translate([0, 0, 1.4])
                difference() {                              // LED Circle
                    cylinder(h = 2, d = 85);
                    translate([0, 0, -1]) cylinder(h = 5, d = 75);
                }
        }
    }
}

// ---- 颈部 ----
module neck() {
    color(C_BODY) translate([0, 8, 25]) rotate([90 - TILT, 0, 0])
        translate([0, 0, -20]) cylinder(h = 40, d = 26);
}

base();
vu_bar();
neck();
head();
