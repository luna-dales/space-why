// Space Why? inner-gimbal completion parts
// Units: mm. Prototype fit/motion hardware only; not validated for live-fire loads.
// Select a part at the bottom and render/export it in OpenSCAD.

$fn = 96;

module slot2d(len, dia) {
    hull() {
        translate([-len/2,0]) circle(d=dia);
        translate([ len/2,0]) circle(d=dia);
    }
}

// Split pillow block for one 608 bearing. Its M5 base holes match one
// outer-ring seam; bearing center is 15 mm above the ring face so it aligns
// with an existing pivot_shaft_lug placed directly on the inner-ring face.
module inner_axis_bearing_pedestal_base() {
    difference() {
        union() {
            translate([0,-25,0]) cube([25,50,5]);
            intersection() {
                translate([5,0,15]) rotate([0,90,0])
                    difference() {
                        cylinder(h=12,d=32,center=true);
                        cylinder(h=14,d=22.3,center=true);
                    }
                translate([-10,-35,-17]) cube([30,70,32]);
            }
            for (y=[-19,19]) translate([-1,y-4,0]) cube([12,8,15]);
        }
        for (y=[-15,15]) translate([12.5,y,-2]) cylinder(h=9,d=5.5);
        for (y=[-19,19]) {
            translate([5,y,-1]) cylinder(h=18,d=4.5);
            translate([5,y,0]) cylinder(h=3.6,r=4.2,$fn=6);
        }
    }
}

module inner_axis_bearing_pedestal_cap() {
    difference() {
        union() {
            intersection() {
                translate([5,0,15]) rotate([0,90,0])
                    difference() {
                        cylinder(h=12,d=32,center=true);
                        cylinder(h=14,d=22.3,center=true);
                    }
                translate([-10,-35,15]) cube([30,70,32]);
            }
            for (y=[-19,19]) translate([-1,y-4,15]) cube([12,8,12]);
        }
        for (y=[-19,19]) translate([5,y,12]) cylinder(h=20,d=4.5);
    }
}

// Two identical plates: one above and one below the inner ring.
// The left/right notches keep the existing pivot lugs directly on the ring.
module inner_ring_clamp_carrier_plate() {
    delta = 15/75*180/PI;
    difference() {
        cylinder(h=5,d=170,center=true);
        cylinder(h=7,d=82,center=true);
        for (x=[-74,74]) translate([x,0,0]) cube([32,56,9],center=true);
        for (a=[45,135,225,315])
            translate([46.25*cos(a),46.25*sin(a),-5]) cylinder(h=10,d=5.5);
        for (seam=[0,90,180,270], sign=[-1,1])
            translate([75*cos(seam+sign*delta),75*sin(seam+sign*delta),-5])
                cylinder(h=10,d=5.5);
    }
}

module rocket_clamp_half_corrected() {
    clamp_id=77; clamp_od=108; clamp_h=20;
    difference() {
        union() {
            intersection() {
                difference() {
                    cylinder(h=clamp_h,d=clamp_od,center=true);
                    cylinder(h=clamp_h+2,d=clamp_id,center=true);
                }
                translate([-clamp_od,-0.01,-clamp_h]) cube([2*clamp_od,clamp_od,2*clamp_h]);
            }
            for (x=[-60,60]) translate([x,9,0]) cube([18,18,20],center=true);
        }
        // Fixed: centered at y=9 so the hole crosses the complete ear.
        for (x=[-60,60]) translate([x,9,0]) rotate([90,0,0]) cylinder(h=22,d=5.4,center=true);
        for (a=[45,135])
            translate([46.25*cos(a),46.25*sin(a),-11]) cylinder(h=22,d=5.4);
    }
}

module inner_axis_shaft_spacer_21_5mm() {
    difference() {
        cylinder(h=21.5,d=14,center=true);
        cylinder(h=23.5,d=8.2,center=true);
    }
}

part = "inner_axis_bearing_pedestal_base";

if (part == "inner_axis_bearing_pedestal_base") inner_axis_bearing_pedestal_base();
if (part == "inner_axis_bearing_pedestal_cap") inner_axis_bearing_pedestal_cap();
if (part == "inner_ring_clamp_carrier_plate") inner_ring_clamp_carrier_plate();
if (part == "rocket_clamp_half_corrected") rocket_clamp_half_corrected();
if (part == "inner_axis_shaft_spacer_21_5mm") inner_axis_shaft_spacer_21_5mm();
