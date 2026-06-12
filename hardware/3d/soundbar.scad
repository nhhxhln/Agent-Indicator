// Agent Indicator — 方案 E「Soundbar」显示器下横条
// 340 × 58 × 54mm,前面板 12° 后仰:左 8×8 Matrix(贴条形区域缩为 4×8 半高横排?
// 保持 8×8 竖排 48mm 缩进),中央 Halo,右侧水平 Usage 条,底缘全宽拾音条。
// 设计意图:塞进显示器支架下方的隙缝,正面信息全部朝向用户。

$fn = 64;

C_BODY   = [0.16, 0.17, 0.19];
C_PANEL  = [0.09, 0.09, 0.11];
C_OFF    = [0.13, 0.13, 0.15];
C_RING   = [0.55, 0.75, 1.0];
C_SCREEN = [0.05, 0.07, 0.12];

W = 340; D = 58; H = 54;
TILT = 12;

module body() {
    color(C_BODY) polyhedron(
        points = [
            [0, 0, 0], [W, 0, 0], [W, D, 0], [0, D, 0],
            [0, D * 0.18, H], [W, D * 0.18, H], [W, D, H], [0, D, H]
        ],
        faces = [
            [0,1,2,3],[4,7,6,5],[0,4,5,1],[1,5,6,2],[2,6,7,3],[3,7,4,0]
        ]);
}

// 前斜面坐标系(u 横向,v 沿面向上)
SL = sqrt(pow(D * 0.18 - 0, 2) + H * H); // 斜面长度近似
module on_face(u, v) {
    translate([u, 0, 0])
        rotate([90 - TILT, 0, 0])
            translate([0, v, -0.5]) children();
}

module matrix_64(cx, cy) {
    cell = 48 / 8; // 紧凑 48mm 版灯距(soundbar 高度受限)
    on_face(cx - 24, cy - 24) {
        color(C_PANEL) cube([48, 48, 1]);
        for (i = [0:7], j = [0:7]) {
            idx = j * 8 + i;
            lit = idx < 38;
            cat = floor(idx / 13) % 5;
            c = !lit ? C_OFF
              : cat == 1 ? [0.95, 0.55, 0.05]
              : cat == 2 ? [0.05, 0.6, 0.8]
              : cat == 3 ? [0.6, 0.1, 0.8]
              : cat == 0 ? [0.35, 0.35, 0.42]
              : [0.05, 0.75, 0.4];
            color(c) translate([i * cell + 0.8, j * cell + 0.8, 1])
                cube([cell - 1.6, cell - 1.6, 0.6]);
        }
    }
}

module halo(cx, cy) {
    on_face(cx, cy) {
        color(C_SCREEN) cylinder(h = 1.2, d = 46);
        color([0.1, 0.35, 0.7]) translate([0, 0, 1.2]) cylinder(h = 0.4, d = 34);
        color(C_RING) translate([0, 0, 0.8])
            difference() {
                cylinder(h = 1.8, d = 47);
                translate([0, 0, -1]) cylinder(h = 5, d = 41);
            }
    }
}

module usage_h(cx, cy) { // 水平 20LED
    seg = 41.5 / 20;
    on_face(cx - 41.5 / 2, cy - 2.5) {
        color(C_PANEL) translate([-1.5, -1.5, 0]) cube([44.5, 8, 0.8]);
        for (k = [0:19]) {
            lit = k < 13;
            c = !lit ? C_OFF : [k / 20, 1 - k / 20, 0.08];
            color(c) translate([k * seg + 0.3, 0, 0.8]) cube([seg - 0.6, 5, 0.6]);
        }
    }
}

module vu(cx, cy) { // 160LED@329mm 全宽拾音条
    n = 160; len = 329; seg = len / n;
    on_face(cx - len / 2, cy - 2) {
        color(C_PANEL) translate([-2, -1.5, 0]) cube([len + 4, 7, 0.8]);
        for (k = [0:n - 1]) {
            dist = abs(k - (n - 1) / 2) / (n / 2);
            lit = dist < 0.35;
            c = !lit ? C_OFF : [0.05, 0.5 + 0.4 * (1 - dist), 0.85];
            color(c) translate([k * seg + 0.15, 0.5, 0.8])
                cube([seg - 0.3, 3.5, 0.5]);
        }
    }
}

body();
matrix_64(60, 32);
halo(170, 32);
usage_h(280, 32);
vu(170, 7);
