// Agent Indicator — 方案 F「Orb」球形桌宠
// Ø110 球体,正面切平嵌 Halo(Circle OD85 + 圆形 UI),
// 底部环形配重座内嵌 24LED 氛围光环;最小可爱形态,仅 Status+IO。

$fn = 96;

C_BODY   = [0.16, 0.17, 0.19];
C_RING   = [0.55, 0.75, 1.0];
C_SCREEN = [0.05, 0.07, 0.12];
C_GLOW   = [0.2, 0.5, 0.9];

TILT = 12;

module orb() {
    translate([0, 0, 64]) rotate([TILT, 0, 0]) {
        // 球体,正面(-y)切平
        color(C_BODY) difference() {
            sphere(d = 110);
            translate([-60, -55 - 41, -60]) cube([120, 60, 120]);
        }
        // Halo 面板
        translate([0, -41.5, 0]) rotate([90, 0, 0]) {
            color(C_SCREEN) cylinder(h = 1.4, d = 86);
            color([0.1, 0.35, 0.7]) translate([0, 0, 1.4])
                cylinder(h = 0.5, d = 62);
            color(C_RING) translate([0, 0, 1])
                difference() {
                    cylinder(h = 2.4, d = 85);
                    translate([0, 0, -1]) cylinder(h = 6, d = 75);
                }
        }
    }
}

module base() {
    color(C_BODY) cylinder(h = 14, d1 = 96, d2 = 78);
    // 底缘氛围光环(透出桌面)
    color(C_GLOW) translate([0, 0, 1])
        difference() {
            cylinder(h = 2.5, d = 97);
            translate([0, 0, -1]) cylinder(h = 6, d = 88);
        }
}

base();
orb();
