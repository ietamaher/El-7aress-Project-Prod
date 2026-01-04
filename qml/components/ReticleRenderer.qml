import QtQuick

Canvas {
    id: canvas

    // Reticle types: 0=BoxCrosshair, 1=BracketsReticle, 2=DuplexCrosshair, 3=FineCrosshair, 4=ChevronReticle
    property int reticleType: 0
    property color color: "#46E2A5"
    property color outlineColor: "#000000"
    property real outlineWidth: 2
    property real currentFov: 45.0

    // LAC-specific properties
    property bool lacActive: false
    property real rangeMeters: 0
    property real confidenceLevel: 1.0 // 0.0 to 1.0

        // M2HB BDC Reticle properties
    property bool showRangeLabels: true
    property bool showStadia: true
    
    // BDC Table: M2 Ball, 100m zero [range_m, drop_mils]
    readonly property var bdcTable: [
        [100,  0.0],    [200,  0.79],  [300,  1.81],  [400,  2.74],
        [500,  3.66],   [600,  4.71],  [700,  5.84],  [800,  7.13],
        [900,  8.44],   [1000, 10.01], [1100, 11.63], [1200, 13.53],
        [1300, 15.58],  [1400, 17.88], [1500, 20.37], [1600, 23.16],
        [1700, 26.27],  [1800, 29.53], [1900, 33.14], [2000, 37.00]
    ]
    readonly property var primaryRanges: [500, 700, 1000, 1200, 1500, 1800, 2000]

    width: 600
    height: 600

    onReticleTypeChanged: requestPaint()
    onColorChanged: requestPaint()
    onCurrentFovChanged: requestPaint()
    onLacActiveChanged: requestPaint()
    onRangeMetersChanged: requestPaint()
    onConfidenceLevelChanged: requestPaint()

    onPaint: {
        var ctx = getContext("2d");
        ctx.reset();

        var centerX = width / 2;
        var centerY = height / 2;

        switch (reticleType) {
            case 0: drawBoxCrosshair(ctx, centerX, centerY); break;
            case 1: drawBracketsReticle(ctx, centerX, centerY); break;
            case 2: drawDuplexCrosshair(ctx, centerX, centerY); break;
            case 3: drawFineCrosshair(ctx, centerX, centerY); break;
            //case 4: drawChevronReticle(ctx, centerX, centerY); break;
            case 4: drawM2BDCReticle(ctx, centerX, centerY); break;

            default: drawBoxCrosshair(ctx, centerX, centerY); break;
        }
    }

    // Helper function to draw with outline
    function drawWithOutline(ctx, drawFunc) {
        // Draw outline
        ctx.strokeStyle = canvas.outlineColor;
        ctx.lineWidth = 2 + outlineWidth;
        ctx.lineCap = "round";
        ctx.lineJoin = "round";
        drawFunc(ctx);

        // Draw main
        ctx.strokeStyle = canvas.color;
        ctx.lineWidth = 2;
        drawFunc(ctx);
    }

   // Helper: Get drop in MIL for given range
    function getDropForRange(range) {
        for (var i = 0; i < bdcTable.length; i++) {
            if (bdcTable[i][0] === range) return bdcTable[i][1];
        }
        return 0;
    }

    // Helper: Draw text with outline
    function drawTextWithOutline(ctx, text, x, y, fontSize) {
        ctx.font = "bold " + fontSize + "px monospace";
        ctx.textAlign = "center";
        ctx.textBaseline = "middle";
        ctx.strokeStyle = canvas.outlineColor;
        ctx.lineWidth = 3;
        ctx.strokeText(text, x, y);
        ctx.fillStyle = canvas.color;
        ctx.fillText(text, x, y);
    }

    // TYPE 0: BOX CROSSHAIR (General purpose - NATO standard)
    function drawBoxCrosshair(ctx, cx, cy) {
        var fovScale = 20.0 / currentFov; // Inverse scale: larger when zoomed in, smaller when zoomed out
        var lineLen = 90 * fovScale;
        var boxSize = Math.min(75 * fovScale, 150);
        var halfBox = boxSize / 2;
        var gap = 2 * fovScale;

        drawWithOutline(ctx, function(c) {
            c.beginPath();
            // Horizontal lines
            c.moveTo(cx - lineLen, cy);
            c.lineTo(cx - halfBox - gap, cy);
            c.moveTo(cx + halfBox + gap, cy);
            c.lineTo(cx + lineLen, cy);

            // Vertical lines
            c.moveTo(cx, cy - lineLen);
            c.lineTo(cx, cy - halfBox - gap);
            c.moveTo(cx, cy + halfBox + gap);
            c.lineTo(cx, cy + lineLen);
            c.stroke();

            // Box
            c.strokeRect(cx - halfBox, cy - halfBox, boxSize, boxSize);
        });

        // Center dot
        drawCenterDot(ctx, cx, cy, 2);
    }

    // TYPE 1: BRACKETS RETICLE (Corner brackets style - Enhanced visibility)
    function drawBracketsReticle(ctx, cx, cy) {
        var fovScale = 15.0 / currentFov; // Inverse scale: larger when zoomed in, smaller when zoomed out
        var crosshairLen = 3 * fovScale;
        var bracketSize = Math.min(Math.max(25 * fovScale, 30), 100);
        var bracketLength = Math.min(Math.max(12 * fovScale, 15), 60);
        var bracketThickness = Math.min(Math.max(2 * fovScale, 2), 4);

        drawWithOutline(ctx, function(c) {
            // Horizontal crosshair
            c.beginPath();
            c.moveTo(cx - crosshairLen, cy);
            c.lineTo(cx + crosshairLen, cy);
            c.stroke();

            // Vertical crosshair
            c.beginPath();
            c.moveTo(cx, cy - crosshairLen);
            c.lineTo(cx, cy + crosshairLen);
            c.stroke();
        });

        // Draw corner brackets using outlined rectangles
        var brackets = [
            // Top-left corner
            {x: cx - bracketSize, y: cy - bracketSize, w: bracketLength, h: bracketThickness},      // Horizontal
            {x: cx - bracketSize, y: cy - bracketSize, w: bracketThickness, h: bracketLength},      // Vertical

            // Top-right corner
            {x: cx + bracketSize - bracketLength, y: cy - bracketSize, w: bracketLength, h: bracketThickness},
            {x: cx + bracketSize - bracketThickness, y: cy - bracketSize, w: bracketThickness, h: bracketLength},

            // Bottom-left corner
            {x: cx - bracketSize, y: cy + bracketSize - bracketThickness, w: bracketLength, h: bracketThickness},
            {x: cx - bracketSize, y: cy + bracketSize - bracketLength, w: bracketThickness, h: bracketLength},

            // Bottom-right corner
            {x: cx + bracketSize - bracketLength, y: cy + bracketSize - bracketThickness, w: bracketLength, h: bracketThickness},
            {x: cx + bracketSize - bracketThickness, y: cy + bracketSize - bracketLength, w: bracketThickness, h: bracketLength}
        ];

        // Draw bracket outlines
        ctx.fillStyle = canvas.outlineColor;
        for (var i = 0; i < brackets.length; i++) {
            ctx.fillRect(
                brackets[i].x - outlineWidth/2,
                brackets[i].y - outlineWidth/2,
                brackets[i].w + outlineWidth,
                brackets[i].h + outlineWidth
            );
        }

        // Draw bracket main
        ctx.fillStyle = canvas.color;
        for (var j = 0; j < brackets.length; j++) {
            ctx.fillRect(brackets[j].x, brackets[j].y, brackets[j].w, brackets[j].h);
        }

        // Center dot
        drawCenterDot(ctx, cx, cy, 2);
    }

    // TYPE 2: DUPLEX CROSSHAIR (Thick outer, thin inner - Sniper style)
    function drawDuplexCrosshair(ctx, cx, cy) {
        var fovScale = 25.0 / currentFov; // Inverse scale: larger when zoomed in, smaller when zoomed out
        var outerLen = Math.min(Math.max(80 * fovScale, 150), 300);
        var innerLen = Math.min(Math.max(15 * fovScale, 50), 100);
        var gap =  Math.min(Math.max(8 * fovScale, 30), 50);
        var thickWidth = 4;
        var thinWidth = 1.5;

        // Thick outer segments with explicit outline (matching thin segments pattern)
        ctx.strokeStyle = canvas.outlineColor;
        ctx.lineWidth = thickWidth + outlineWidth;
        ctx.lineCap = "round";
        ctx.lineJoin = "round";
        ctx.beginPath();
        // Horizontal - left thick
        ctx.moveTo(cx - outerLen, cy);
        ctx.lineTo(cx - gap - innerLen, cy);
        // Horizontal - right thick
        ctx.moveTo(cx + gap + innerLen, cy);
        ctx.lineTo(cx + outerLen, cy);
        // Vertical - top thick
        ctx.moveTo(cx, cy - outerLen);
        ctx.lineTo(cx, cy - gap - innerLen);
        // Vertical - bottom thick
        ctx.moveTo(cx, cy + gap + innerLen);
        ctx.lineTo(cx, cy + outerLen);
        ctx.stroke();

        ctx.strokeStyle = canvas.color;
        ctx.lineWidth = thickWidth;
        ctx.beginPath();
        // Horizontal - left thick
        ctx.moveTo(cx - outerLen, cy);
        ctx.lineTo(cx - gap - innerLen, cy);
        // Horizontal - right thick
        ctx.moveTo(cx + gap + innerLen, cy);
        ctx.lineTo(cx + outerLen, cy);
        // Vertical - top thick
        ctx.moveTo(cx, cy - outerLen);
        ctx.lineTo(cx, cy - gap - innerLen);
        // Vertical - bottom thick
        ctx.moveTo(cx, cy + gap + innerLen);
        ctx.lineTo(cx, cy + outerLen);
        ctx.stroke();

        // Thin inner segments
        ctx.strokeStyle = canvas.outlineColor;
        ctx.lineWidth = thinWidth + outlineWidth;
        ctx.beginPath();
        // Horizontal inner
        ctx.moveTo(cx - gap - innerLen, cy);
        ctx.lineTo(cx - gap, cy);
        ctx.moveTo(cx + gap, cy);
        ctx.lineTo(cx + gap + innerLen, cy);
        // Vertical inner
        ctx.moveTo(cx, cy - gap - innerLen);
        ctx.lineTo(cx, cy - gap);
        ctx.moveTo(cx, cy + gap);
        ctx.lineTo(cx, cy + gap + innerLen);
        ctx.stroke();

        ctx.strokeStyle = canvas.color;
        ctx.lineWidth = thinWidth;
        ctx.beginPath();
        // Horizontal inner
        ctx.moveTo(cx - gap - innerLen, cy);
        ctx.lineTo(cx - gap, cy);
        ctx.moveTo(cx + gap, cy);
        ctx.lineTo(cx + gap + innerLen, cy);
        // Vertical inner
        ctx.moveTo(cx, cy - gap - innerLen);
        ctx.lineTo(cx, cy - gap);
        ctx.moveTo(cx, cy + gap);
        ctx.lineTo(cx, cy + gap + innerLen);
        ctx.stroke();

        // Center dot
        drawCenterDot(ctx, cx, cy, 1.5);
    }

    // TYPE 3: FINE CROSSHAIR (Thin precision crosshair - Long range)
    function drawFineCrosshair(ctx, cx, cy) {
        var fovScale = 25.0 / currentFov; // Inverse scale: larger when zoomed in, smaller when zoomed out
        var lineLen = 90 * fovScale;
        var gap = 6 * fovScale;
        var lineWidth = 1.5;
        var tickLength = 4 * fovScale;
        var tickSpacing = 0;//5 * fovScale; // Spacing for range ticks

        // Main crosshair lines
        ctx.strokeStyle = canvas.outlineColor;
        ctx.lineWidth = lineWidth + outlineWidth;
        ctx.lineCap = "round";
        ctx.beginPath();
        // Horizontal
        ctx.moveTo(cx - lineLen, cy);
        ctx.lineTo(cx - gap, cy);
        ctx.moveTo(cx + gap, cy);
        ctx.lineTo(cx + lineLen, cy);
        // Vertical
        ctx.moveTo(cx, cy - lineLen);
        ctx.lineTo(cx, cy - gap);
        ctx.moveTo(cx, cy + gap);
        ctx.lineTo(cx, cy + lineLen);
        ctx.stroke();

        ctx.strokeStyle = canvas.color;
        ctx.lineWidth = lineWidth;
        ctx.beginPath();
        // Horizontal
        ctx.moveTo(cx - lineLen, cy);
        ctx.lineTo(cx - gap, cy);
        ctx.moveTo(cx + gap, cy);
        ctx.lineTo(cx + lineLen, cy);
        // Vertical
        ctx.moveTo(cx, cy - lineLen);
        ctx.lineTo(cx, cy - gap);
        ctx.moveTo(cx, cy + gap);
        ctx.lineTo(cx, cy + lineLen);
        ctx.stroke();

        // Range estimation ticks (every 15 pixels on vertical axis below center)
        /*for (var i = 1; i <= 4; i++) {
            var tickY = cy + gap + (i * tickSpacing);
            if (tickY < cy + lineLen) {
                ctx.strokeStyle = canvas.outlineColor;
                ctx.lineWidth = lineWidth + outlineWidth;
                ctx.beginPath();
                ctx.moveTo(cx - tickLength, tickY);
                ctx.lineTo(cx + tickLength, tickY);
                ctx.stroke();

                ctx.strokeStyle = canvas.color;
                ctx.lineWidth = lineWidth;
                ctx.beginPath();
                ctx.moveTo(cx - tickLength, tickY);
                ctx.lineTo(cx + tickLength, tickY);
                ctx.stroke();
            }
        }*/

        // Center dot
        drawCenterDot(ctx, cx, cy, 1.5);
    }

    // TYPE 4: CHEVRON RETICLE (Upward pointing chevron - CQB/ACOG style)
    function drawChevronReticle(ctx, cx, cy) {
        var fovScale = 45.0 / currentFov; // Inverse scale: larger when zoomed in, smaller when zoomed out

        // MIL-based calibration (1024 px canvas width as baseline  x 768px height)  
        var baseCanvasWidth = 1024;
        var milsInFOV = currentFov * 17.4533; // Convert FOV degrees to milliradians (1° = 17.4533 mils)
        var pixelsPerMil = baseCanvasWidth / milsInFOV;

        var chevronHeight = 2.0 * pixelsPerMil; // 2 MIL height
        var chevronWidth = 1.5 * pixelsPerMil; // 1.5 MIL width
        var lineLen = 4.0 * pixelsPerMil; // 4 MIL horizontal lines
        var gap = 0.3 * pixelsPerMil; // 0.3 MIL gap

        // Chevron pointing UP (tip at top)
        drawWithOutline(ctx, function(c) {
            c.beginPath();
            // Left side of chevron (from bottom-left to tip)
            c.moveTo(cx - chevronWidth, cy + chevronHeight);
            c.lineTo(cx, cy);
            // Right side of chevron (from tip to bottom-right)
            c.lineTo(cx + chevronWidth, cy + chevronHeight);
            c.stroke();
        });

        // Horizontal reference lines at chevron base
        drawWithOutline(ctx, function(c) {
            c.beginPath();
            // Left horizontal
            c.moveTo(cx - lineLen, cy + chevronHeight);
            c.lineTo(cx - chevronWidth - gap, cy + chevronHeight);
            // Right horizontal
            c.moveTo(cx + chevronWidth + gap, cy + chevronHeight);
            c.lineTo(cx + lineLen, cy + chevronHeight);
            c.stroke();
        });

        // Vertical drop line (for holdover reference below chevron)
        var dropLineLen = 4.0 * pixelsPerMil; // 4 MIL drop line
        drawWithOutline(ctx, function(c) {
            c.beginPath();
            c.moveTo(cx, cy + chevronHeight + gap);
            c.lineTo(cx, cy + chevronHeight + dropLineLen);
            c.stroke();
        });

        // MIL-calibrated holdover tick marks (1 MIL intervals)
        var tickLength = 0.4 * pixelsPerMil; // 0.4 MIL tick length
        var milInterval = 1.0 * pixelsPerMil; // 1 MIL spacing between ticks

        for (var i = 1; i <= 4; i++) {
            var tickY = cy + chevronHeight + gap + (i * milInterval);
            if (tickY < cy + chevronHeight + dropLineLen) {
                ctx.strokeStyle = canvas.outlineColor;
                ctx.lineWidth = 2 + outlineWidth;
                ctx.beginPath();
                ctx.moveTo(cx - tickLength, tickY);
                ctx.lineTo(cx + tickLength, tickY);
                ctx.stroke();

                ctx.strokeStyle = canvas.color;
                ctx.lineWidth = 2;
                ctx.beginPath();
                ctx.moveTo(cx - tickLength, tickY);
                ctx.lineTo(cx + tickLength, tickY);
                ctx.stroke();
            }
        }

        // Tip dot at chevron point (at top)
        drawCenterDot(ctx, cx, cy, 2);
    }

    // TYPE 5: M2HB BDC RETICLE (MIL-accurate ballistic drop compensation)
    function drawM2BDCReticle(ctx, cx, cy) {
        // MIL calibration for 1024px baseline
        var baseCanvasWidth = 1024;
        var milsInFOV = currentFov * 17.453293;
        var pxPerMil = baseCanvasWidth / milsInFOV;

        // === CENTER CHEVRON (100m aiming point) ===
        var chevronWidth = Math.max(1.0 * pxPerMil, 8);
        var chevronHeight = Math.max(1.5 * pxPerMil, 12);

        drawWithOutline(ctx, function(c) {
            c.beginPath();
            c.moveTo(cx - chevronWidth, cy + chevronHeight);
            c.lineTo(cx, cy);
            c.lineTo(cx + chevronWidth, cy + chevronHeight);
            c.stroke();
        });

        // Center dot at tip
        drawCenterDot(ctx, cx, cy, 3);

        // === HORIZONTAL REFERENCE LINES ===
        var hLineLen = Math.max(4.0 * pxPerMil, 30);
        var hLineGap = Math.max(1.5 * pxPerMil, 12);

        drawWithOutline(ctx, function(c) {
            c.beginPath();
            c.moveTo(cx - hLineGap, cy);
            c.lineTo(cx - hLineGap - hLineLen, cy);
            c.moveTo(cx + hLineGap, cy);
            c.lineTo(cx + hLineGap + hLineLen, cy);
            c.stroke();
        });

        // === BDC VERTICAL DROP LINE ===
        var maxDrop = getDropForRange(2000);  // 37 MIL
        var startY = cy + chevronHeight + Math.max(0.5 * pxPerMil, 5);
        var endY = cy + (maxDrop * pxPerMil) + (2 * pxPerMil);
        endY = Math.min(endY, canvas.height - 20);  // Clip to screen

        drawWithOutline(ctx, function(c) {
            c.beginPath();
            c.moveTo(cx, startY);
            c.lineTo(cx, endY);
            c.stroke();
        });

        // === BDC RANGE TICK MARKS ===
        for (var i = 0; i < primaryRanges.length; i++) {
            var range = primaryRanges[i];
            var dropMils = getDropForRange(range);
            var dropY = cy + (dropMils * pxPerMil);

            if (dropY > canvas.height - 10) continue;

            // Major marks (500m intervals) are wider
            var tickHalfWidth = (range % 500 === 0) ?
                Math.max(1.5 * pxPerMil, 12) : Math.max(1.0 * pxPerMil, 8);

            drawWithOutline(ctx, function(c) {
                c.beginPath();
                c.moveTo(cx - tickHalfWidth, dropY);
                c.lineTo(cx + tickHalfWidth, dropY);
                c.stroke();
            });

            // Range labels (abbreviated: 5=500m, 10=1000m, etc.)
            if (showRangeLabels && pxPerMil > 1.5) {
                var fontSize = Math.max(9, Math.min(14, pxPerMil * 1.5));
                var labelText = (range / 100).toFixed(0);
                drawTextWithOutline(ctx, labelText, cx + tickHalfWidth + fontSize + 2, dropY, fontSize);
            }
        }

        // === STADIAMETRIC RANGING BRACKETS (1.7m standing man) ===
        if (showStadia) {
            var stadiaRanges = [500, 700, 1000, 1500];
            var stadiaX = cx - hLineGap - hLineLen - Math.max(2 * pxPerMil, 15);

            for (var s = 0; s < stadiaRanges.length; s++) {
                var stadiaRange = stadiaRanges[s];
                // Angular size: (1.7m × 1000) / range_m
                var stadiaMils = (1.7 * 1000.0) / stadiaRange;
                var stadiaHeightPx = stadiaMils * pxPerMil;

                if (stadiaHeightPx < 6 || stadiaX < 50) continue;

                var bracketWidth = Math.max(0.3 * pxPerMil, 4);
                var bracketY = cy - stadiaHeightPx / 2;

                drawWithOutline(ctx, function(c) {
                    c.beginPath();
                    c.moveTo(stadiaX - bracketWidth, bracketY);
                    c.lineTo(stadiaX, bracketY);
                    c.lineTo(stadiaX, bracketY + stadiaHeightPx);
                    c.lineTo(stadiaX - bracketWidth, bracketY + stadiaHeightPx);
                    c.stroke();
                });

                // Stadia range label
                if (pxPerMil > 2.0) {
                    var sFontSize = Math.max(8, Math.min(11, pxPerMil));
                    drawTextWithOutline(ctx, (stadiaRange/100).toFixed(0),
                        stadiaX - bracketWidth - sFontSize - 2, cy, sFontSize);
                }

                stadiaX -= (bracketWidth * 4 + Math.max(pxPerMil, 8));
            }
        }

        // === 1-MIL WINDAGE/LEAD HASHES ===
// === 1-MIL WINDAGE/LEAD HASHES ===
        // Wind values at 1000m: 1 MIL ≈ 2.2 m/s crosswind
        var windPerMil_1000m = [2.2, 4.4, 6.5, 8.7];  // m/s for 1,2,3,4 MIL at 1000m

        if (pxPerMil > 2.5) {
            var hashLen = Math.max(0.5 * pxPerMil, 4);
            for (var m = 1; m <= 4; m++) {
                var hashX = m * pxPerMil;
                drawWithOutline(ctx, function(c) {
                    c.beginPath();
                    c.moveTo(cx - hLineGap - hashX, cy - hashLen);
                    c.lineTo(cx - hLineGap - hashX, cy + hashLen);
                    c.moveTo(cx + hLineGap + hashX, cy - hashLen);
                    c.lineTo(cx + hLineGap + hashX, cy + hashLen);
                    c.stroke();
                });

                // Wind labels (right side only, below hash) - show at 1000m ref
                if (pxPerMil > 4.0) {
                    var windFontSize = Math.max(7, Math.min(10, pxPerMil * 0.8));
                    var windLabel = windPerMil_1000m[m-1].toFixed(0);
                    drawTextWithOutline(ctx, windLabel, cx + hLineGap + hashX, cy + hashLen + windFontSize, windFontSize);
                }
            }

            // "m/s @1k" label at end
            if (pxPerMil > 5.0) {
                var refFontSize = Math.max(6, Math.min(9, pxPerMil * 0.6));
                drawTextWithOutline(ctx, "@1k", cx + hLineGap + (4 * pxPerMil) + refFontSize + 4, cy, refFontSize);
            }
        }
    }
    // Helper function to draw center dot with outline
    function drawCenterDot(ctx, x, y, radius) {
        // Outline
        ctx.fillStyle = canvas.outlineColor;
        ctx.beginPath();
        ctx.arc(x, y, radius + outlineWidth/2, 0, 2 * Math.PI);
        ctx.fill();

        // Main dot
        ctx.fillStyle = canvas.color;
        ctx.beginPath();
        ctx.arc(x, y, radius, 0, 2 * Math.PI);
        ctx.fill();
    }
}
