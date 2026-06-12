// Agent Indicator — 方案 B「Console」横向一体桌搭
// 176 × 112 × 42mm 楔形(前缘 18mm),与 docs/03 §方案 B 尺寸一致
// 渲染:xvfb-run openscad -o console.png --imgsize=1600,1200 console.scad

$fn = 64;

W = 176;        // 宽
D = 112;        // 深
H_BACK = 42;    // 后部高
H_FRONT = 18;   // 前缘高
SLOPE = atan((H_BACK - H_FRONT) / D);  // ≈12.1°
SLANT = sqrt(D * D + pow(H_BACK - H_FRONT, 2));

C_BODY   = [0.16, 0.17, 0.19];
C_PANEL  = [0.09, 0.09, 0.11];
C_OFF    = [0.13, 0.13, 0.15];
C_RING   = [0.55, 0.75, 1.0];
C_SCREEN = [0.05, 0.07, 0.12];

// 斜面坐标系:u=横向(0..W),v=沿斜面从前缘起(0..SLANT),children 立在面上
module on_face(u, v) {
    translate([u, v * cos(SLOPE), H_FRONT + v * sin(SLOPE)])
        rotate([SLOPE, 0, 0]) children();
}

// ---- 机体 ----
module body() {
    color(C_BODY) polyhedron(
        points = [
            [0, 0, 0], [W, 0, 0], [W, D, 0], [0, D, 0],
            [0, 0, H_FRONT], [W, 0, H_FRONT], [W, D, H_BACK], [0, D, H_BACK]
        ],
        faces = [
            [0, 1, 2, 3], [4, 7, 6, 5], [0, 4, 5, 1],
            [1, 5, 6, 2], [2, 6, 7, 3], [3, 7, 4, 0]
        ]);
}

// ---- 8×8 Matrix(context 热力图样式) ----
module matrix(cx, cv) {
    cell = 64 / 8;
    on_face(cx - 32, cv - 32) {
        color(C_PANEL) cube([64, 64, 1.2]);
        for (i = [0:7], j = [0:7]) {
            idx = j * 8 + i;
            lit = idx < 38;                       // ~60% context 已用
            cat = floor(idx / 13) % 5;
            c = !lit ? C_OFF
              : cat == 0 ? [0.35, 0.35, 0.42]     // system
              : cat == 1 ? [0.95, 0.55, 0.05]     // tools
              : cat == 2 ? [0.05, 0.6, 0.8]       // mcp
              : cat == 3 ? [0.6, 0.1, 0.8]        // memory
              : [0.05, 0.75, 0.4];                // messages
            color(c) translate([i * cell + 1, j * cell + 1, 1.2])
                cube([cell - 2, cell - 2, 0.8]);
        }
    }
}

// ---- Halo:LED Circle(OD85/ID75)套圆形显示 ----
module halo(cx, cv) {
    on_face(cx, cv) {
        color(C_SCREEN) cylinder(h = 1.5, d = 84);          // 屏面(被环遮角)
        color([0.1, 0.35, 0.7]) translate([0, 0, 1.5])
            cylinder(h = 0.4, d = 60);                       // 屏幕亮区示意
        color(C_RING) translate([0, 0, 1])
            difference() {
                cylinder(h = 2.2, d = 85);
                translate([0, 0, -1]) cylinder(h = 5, d = 75);
            }
        color(C_BODY) translate([0, 0, 0])
            difference() {                                   // 外饰圈
                cylinder(h = 3.4, d = 92);
                translate([0, 0, -1]) cylinder(h = 6, d = 85.5);
            }
    }
}

// ---- 竖直 Usage 条(20 段) ----
module usage_bar(cx, cv) {
    seg = 41.5 / 20;
    on_face(cx - 2.5, cv - 41.5 / 2) {
        color(C_PANEL) translate([-1.5, -1.5, 0]) cube([8, 44.5, 1]);
        for (k = [0:19]) {
            lit = k < 13;                                    // 65%
            c = !lit ? C_OFF : [k / 20, 1 - k / 20, 0.08];   // 绿→红
            color(c) translate([0, k * seg + 0.3, 1])
                cube([5, seg - 0.6, 0.8]);
        }
    }
}

// ---- 拾音条(64 LED @132mm,中心展开 VU) ----
module vu_bar(cx, cv) {
    n = 64; len = 132; seg = len / n;
    on_face(cx - len / 2, cv - 2.5) {
        color(C_PANEL) translate([-2, -1.5, 0]) cube([len + 4, 8, 1]);
        for (k = [0:n - 1]) {
            dist = abs(k - (n - 1) / 2) / (n / 2);
            lit = dist < 0.45;                               // 中心亮
            c = !lit ? C_OFF : [0.05, 0.5 + 0.4 * (1 - dist), 0.85];
            color(c) translate([k * seg + 0.25, 0, 1])
                cube([seg - 0.5, 5, 0.8]);
        }
    }
}

// ---- MIC 孔 ----
module mic_hole(cx, cv) {
    on_face(cx, cv) color([0.02, 0.02, 0.02]) cylinder(h = 2, d = 2);
}

body();
matrix(40, 72);          // 左:8×8 Matrix
usage_bar(79, 72);       // 中:Usage 竖条
halo(128, 72);           // 右:Halo(Circle+LCD)
vu_bar(88, 14);          // 前缘:拾音条
mic_hole(16, 14);
mic_hole(160, 14);
