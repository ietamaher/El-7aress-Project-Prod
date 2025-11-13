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

    width: 200
    height: 200

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
            case 4: drawChevronReticle(ctx, centerX, centerY); break;
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

    // TYPE 0: BOX CROSSHAIR (General purpose - NATO standard)
    function drawBoxCrosshair(ctx, cx, cy) {
        var fovScale = currentFov / 45.0; // Scale based on FOV (45° baseline)
        var lineLen = 80 * fovScale;
        var boxSize = 50 * fovScale;
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
        var fovScale = currentFov / 45.0; // Scale based on FOV (45° baseline)
        var crosshairLen = 30 * fovScale;
        var bracketSize = 25 * fovScale;
        var bracketLength = 12 * fovScale;
        var bracketThickness = 2 * fovScale;

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
        var fovScale = currentFov / 45.0; // Scale based on FOV (45° baseline)
        var outerLen = 80 * fovScale;
        var innerLen = 15 * fovScale;
        var gap = 8 * fovScale;
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
        var fovScale = currentFov / 45.0; // Scale based on FOV (45° baseline)
        var lineLen = 90 * fovScale;
        var gap = 6 * fovScale;
        var lineWidth = 1.5;
        var tickLength = 4 * fovScale;
        var tickSpacing = 15 * fovScale; // Spacing for range ticks

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
        for (var i = 1; i <= 4; i++) {
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
        }

        // Center dot
        drawCenterDot(ctx, cx, cy, 1.5);
    }

    // TYPE 4: CHEVRON RETICLE (Downward pointing chevron - CQB style)
    function drawChevronReticle(ctx, cx, cy) {
        var fovScale = currentFov / 45.0; // Scale based on FOV (45° baseline)
        var chevronHeight = 25 * fovScale;
        var chevronWidth = 18 * fovScale;
        var lineLen = 60 * fovScale;
        var gap = 5 * fovScale;

        // Chevron pointing down
        drawWithOutline(ctx, function(c) {
            c.beginPath();
            // Left side of chevron
            c.moveTo(cx - chevronWidth, cy - chevronHeight);
            c.lineTo(cx, cy);
            // Right side of chevron
            c.lineTo(cx + chevronWidth, cy - chevronHeight);
            c.stroke();
        });

        // Horizontal reference lines
        drawWithOutline(ctx, function(c) {
            c.beginPath();
            // Left horizontal
            c.moveTo(cx - lineLen, cy);
            c.lineTo(cx - chevronWidth - gap, cy);
            // Right horizontal
            c.moveTo(cx + chevronWidth + gap, cy);
            c.lineTo(cx + lineLen, cy);
            c.stroke();
        });

        // Vertical drop line (for holdover reference)
        drawWithOutline(ctx, function(c) {
            c.beginPath();
            c.moveTo(cx, cy + gap);
            c.lineTo(cx, cy + lineLen);
            c.stroke();
        });

        // Range tick marks on vertical line (below chevron)
        var tickLength = 3 * fovScale;
        var tickSpacing = 12 * fovScale;
        for (var i = 1; i <= 4; i++) {
            var tickY = cy + gap + (i * tickSpacing);
            if (tickY < cy + lineLen) {
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

        // Tip dot at chevron point
        drawCenterDot(ctx, cx, cy, 2);
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
