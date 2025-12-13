import QtQuick

Canvas {
    id: canvas

    property var viewModel: null

    // Redraw when data changes
    onViewModelChanged: requestPaint()

    Connections {
        target: viewModel
        function onGimbalAzChanged() { canvas.requestPaint() }
        function onGimbalElChanged() { canvas.requestPaint() }
        function onAreaZonesChanged() { canvas.requestPaint() }
        function onSectorScansChanged() { canvas.requestPaint() }
        function onTrpsChanged() { canvas.requestPaint() }
        function onWipZoneChanged() { canvas.requestPaint() }
        function onHasWipZoneChanged() { canvas.requestPaint() }
        function onHighlightedZoneIdChanged() { canvas.requestPaint() }
    }

    onPaint: {
        if (!viewModel) return

        var ctx = getContext("2d")
        ctx.reset()

        // Background
        ctx.fillStyle = "#282828"
        ctx.fillRect(0, 0, width, height)

        // Grid
        drawGrid(ctx)
        drawAxesLabels(ctx)

        // Saved zones
        drawAreaZones(ctx, viewModel.areaZones)
        drawSectorScans(ctx, viewModel.sectorScans)
        drawTRPs(ctx, viewModel.trps)

        // WIP zone
        if (viewModel.hasWipZone) {
            drawWipZone(ctx)
        }

        // Gimbal indicator (on top)
        drawGimbalIndicator(ctx)
    }

    function drawGrid(ctx) {
        ctx.strokeStyle = "#505050"
        ctx.lineWidth = 1

        // Azimuth grid lines (every 30°)
        for (var az = 0; az <= 360; az += 30) {
            var x = az / 360.0 * width
            ctx.beginPath()
            ctx.moveTo(x, 0)
            ctx.lineTo(x, height)
            ctx.stroke()
        }

        // Elevation grid lines (every 10°)
        // Gimbal operates between -15° and +46°, so display -20° to 60° for margin
        var elMin = -20
        var elMax = 60
        var elRange = elMax - elMin
        for (var el = elMin; el <= elMax; el += 10) {
            var y = height - ((el - elMin) / elRange * height)
            ctx.beginPath()
            ctx.moveTo(0, y)
            ctx.lineTo(width, y)
            ctx.stroke()
        }
    }

    function drawAxesLabels(ctx) {
        ctx.fillStyle = "white"
        ctx.font = "10px sans-serif"

        // Azimuth labels
        for (var az = 0; az <= 360; az += 60) {
            var x = az / 360.0 * width
            ctx.fillText(az + "°", x - 10, height - 5)
        }

        // Elevation labels
        // Gimbal operates between -15° and +46°, so display -20° to 60° for margin
        var elMin = -20
        var elMax = 60
        var elRange = elMax - elMin
        for (var el = elMin; el <= elMax; el += 20) {
            var y = height - ((el - elMin) / elRange * height)
            ctx.fillText(el + "°", 5, y + 5)
        }

        // Axis titles
        ctx.fillText("Azimuth (0-360°)", width / 2 - 40, height - 20)
        ctx.save()
        ctx.translate(15, height / 2)
        ctx.rotate(-Math.PI / 2)
        ctx.fillText("Elevation", -25, 0)
        ctx.restore()
    }

    function drawGimbalIndicator(ctx) {
        if (!viewModel) return

        var pos = viewModel.azElToPixel(
            viewModel.gimbalAz,
            viewModel.gimbalEl,
            width,
            height
        )

        ctx.strokeStyle = "yellow"
        ctx.lineWidth = 2
        ctx.fillStyle = "yellow"

        // Crosshair
        ctx.beginPath()
        ctx.moveTo(pos.x - 10, pos.y)
        ctx.lineTo(pos.x + 10, pos.y)
        ctx.moveTo(pos.x, pos.y - 10)
        ctx.lineTo(pos.x, pos.y + 10)
        ctx.stroke()

        // Center dot
        ctx.beginPath()
        ctx.arc(pos.x, pos.y, 3, 0, 2 * Math.PI)
        ctx.fill()
    }

    function drawAreaZones(ctx, zones) {
        for (var i = 0; i < zones.length; i++) {
            var zone = zones[i]
            if (!zone.isEnabled) continue

            var color = getZoneColor(zone.type)
            var isHighlighted = (zone.id === viewModel.highlightedZoneId)

            drawZoneRect(ctx, zone, color, isHighlighted, false)
        }
    }

    function drawZoneRect(ctx, zone, color, isHighlighted, isWip) {
        var startAz = viewModel.normalizeAzimuth(zone.startAzimuth)
        var endAz = viewModel.normalizeAzimuth(zone.endAzimuth)
        var wrapsAround = (startAz > endAz)

        if (!wrapsAround) {
            drawSingleZoneRect(ctx, startAz, endAz, zone.minElevation, zone.maxElevation,
                             color, isHighlighted, isWip, zone.isOverridable, zone.id)
        } else {
            // Draw two parts for wrap-around
            drawSingleZoneRect(ctx, startAz, 360, zone.minElevation, zone.maxElevation,
                             color, isHighlighted, isWip, zone.isOverridable, zone.id)
            drawSingleZoneRect(ctx, 0, endAz, zone.minElevation, zone.maxElevation,
                             color, isHighlighted, isWip, zone.isOverridable, zone.id)
        }
    }

    function drawSingleZoneRect(ctx, startAz, endAz, minEl, maxEl, color, isHighlighted, isWip, isOverridable, zoneId) {
        var topLeft = viewModel.azElToPixel(startAz, maxEl, width, height)
        var bottomRight = viewModel.azElToPixel(endAz, minEl, width, height)

        var rectWidth = bottomRight.x - topLeft.x
        var rectHeight = bottomRight.y - topLeft.y

        // Fill
        ctx.fillStyle = color + "33"  // Add alpha
        ctx.fillRect(topLeft.x, topLeft.y, rectWidth, rectHeight)

        // Border
        ctx.strokeStyle = isHighlighted ? Qt.lighter(color, 1.5) : color
        ctx.lineWidth = isHighlighted ? 3 : 2
        ctx.setLineDash(isWip ? [5, 5] : (isOverridable ? [5, 3] : []))
        ctx.strokeRect(topLeft.x, topLeft.y, rectWidth, rectHeight)
        ctx.setLineDash([])

        // ID label (only if not WIP and has space)
        if (!isWip && rectWidth > 30 && rectHeight > 15) {
            ctx.fillStyle = "white"
            ctx.font = "10px sans-serif"
            ctx.fillText("ID:" + zoneId, topLeft.x + 5, topLeft.y + 15)
        }
    }

    function drawSectorScans(ctx, scans) {
        for (var i = 0; i < scans.length; i++) {
            var scan = scans[i]
            if (!scan.isEnabled) continue

            var p1 = viewModel.azElToPixel(scan.az1, scan.el1, width, height)
            var p2 = viewModel.azElToPixel(scan.az2, scan.el2, width, height)

            // Check if line crosses 0/360° boundary
            var az1 = viewModel.normalizeAzimuth(scan.az1)
            var az2 = viewModel.normalizeAzimuth(scan.az2)
            var crossesZero = (az1 > az2) && ((az1 - az2) > 180.0)

            ctx.strokeStyle = "#4A90E2"
            ctx.lineWidth = 2

            if (crossesZero) {
                // Calculate interpolated elevation at 0° and 360°
                var totalAzSpan = (360.0 - az1) + az2
                var elAtZero = scan.el1 + (scan.el2 - scan.el1) * (360.0 - az1) / totalAzSpan

                var pZero = viewModel.azElToPixel(0.0, elAtZero, width, height)
                var p360 = viewModel.azElToPixel(359.9, elAtZero, width, height)

                // Draw two segments
                ctx.beginPath()
                ctx.moveTo(p1.x, p1.y)
                ctx.lineTo(p360.x, p360.y)
                ctx.stroke()

                ctx.beginPath()
                ctx.moveTo(pZero.x, pZero.y)
                ctx.lineTo(p2.x, p2.y)
                ctx.stroke()

                // Markers at main endpoints
                ctx.fillStyle = "#4A90E2"
                ctx.beginPath()
                ctx.arc(p1.x, p1.y, 3, 0, 2 * Math.PI)
                ctx.arc(p2.x, p2.y, 3, 0, 2 * Math.PI)
                ctx.fill()

                // ID label
                var midPoint = {x: (p1.x + p360.x) / 2, y: (p1.y + p360.y) / 2}
                ctx.fillStyle = "white"
                ctx.font = "10px sans-serif"
                ctx.fillText("ID:" + scan.id, midPoint.x + 5, midPoint.y - 5)
            } else {
                // Normal case: single line
                ctx.beginPath()
                ctx.moveTo(p1.x, p1.y)
                ctx.lineTo(p2.x, p2.y)
                ctx.stroke()

                // Endpoints
                ctx.fillStyle = "#4A90E2"
                ctx.beginPath()
                ctx.arc(p1.x, p1.y, 3, 0, 2 * Math.PI)
                ctx.arc(p2.x, p2.y, 3, 0, 2 * Math.PI)
                ctx.fill()

                // ID label
                var midPoint = {x: (p1.x + p2.x) / 2, y: (p1.y + p2.y) / 2}
                ctx.fillStyle = "white"
                ctx.font = "10px sans-serif"
                ctx.fillText("ID:" + scan.id, midPoint.x + 5, midPoint.y - 5)
            }
        }
    }

    function drawTRPs(ctx, trps) {
        for (var i = 0; i < trps.length; i++) {
            var trp = trps[i]
            var pos = viewModel.azElToPixel(trp.azimuth, trp.elevation, width, height)

            ctx.strokeStyle = "yellow"
            ctx.lineWidth = 2

            // Cross marker
            ctx.beginPath()
            ctx.moveTo(pos.x - 6, pos.y)
            ctx.lineTo(pos.x + 6, pos.y)
            ctx.moveTo(pos.x, pos.y - 6)
            ctx.lineTo(pos.x, pos.y + 6)
            ctx.stroke()

            // ID label
            ctx.fillStyle = "white"
            ctx.font = "10px sans-serif"
            ctx.fillText("ID:" + trp.id, pos.x + 8, pos.y - 8)
        }
    }

    function drawWipZone(ctx) {
        var wip = viewModel.wipZone
        var type = viewModel.wipZoneType

        ctx.strokeStyle = "#00FF99"  // Green for WIP
        ctx.lineWidth = 2
        ctx.setLineDash([5, 5])

        if (type === 1) {  // AreaZone
            drawZoneRect(ctx, wip, "#00FF99", false, true)
        } else if (type === 2) {  // SectorScan
            var p1 = viewModel.azElToPixel(wip.az1, wip.el1, width, height)
            var p2 = viewModel.azElToPixel(wip.az2, wip.el2, width, height)

            ctx.beginPath()
            ctx.moveTo(p1.x, p1.y)
            ctx.lineTo(p2.x, p2.y)
            ctx.stroke()

            ctx.fillStyle = "#00FF99"
            ctx.beginPath()
            ctx.arc(p1.x, p1.y, 4, 0, 2 * Math.PI)
            ctx.arc(p2.x, p2.y, 4, 0, 2 * Math.PI)
            ctx.fill()
        } else if (type === 3) {  // TRP
            var pos = viewModel.azElToPixel(wip.azimuth, wip.elevation, width, height)

            ctx.beginPath()
            ctx.moveTo(pos.x - 8, pos.y)
            ctx.lineTo(pos.x + 8, pos.y)
            ctx.moveTo(pos.x, pos.y - 8)
            ctx.lineTo(pos.x, pos.y + 8)
            ctx.stroke()
        }

        ctx.setLineDash([])
    }

    function getZoneColor(type) {
        // type: 1=Safety, 2=NoTraverse, 3=NoFire
        switch(type) {
            case 1: return "#00FFFF"  // Cyan for Safety
            case 2: return "#C81428"  // Red for NoTraverse
            case 3: return "#FF00FF"  // Magenta for NoFire
            default: return "#808080" // Gray
        }
    }
}
