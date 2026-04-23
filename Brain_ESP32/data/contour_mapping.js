/**
 * ContourMapGenerator - canvas rendering version
 * adapts the provided controur map generator for use on the embedded
 * web server. Because the ESP32 creates an isolated wifi access point
 * with no internet connectivity, the original Plotly.js CDN dependency
 * cannot be used. This version replicates the identical algorithm
 * 
 * The original generatePlotlyContour() and generateGradientMap() signatures
 * are preserved so this file can serve as a drop in replacement for the companies
 * desktop viewer as well - Plotly is simply called when it is available
 */

class ContourMapGenerator
{
    constructor(options = {})
    {
        this.gridSize = options.gridSize || 1;
        this.contourMin = options.contourMin !== undefined ? options.contourMin : -350;
        this.contourMax = options.contourMax !== undefined ? options.contourMax : -50;
        this.gradientMin = options.gradientMin !== undefined ? options.gradientMin : -200;
        this.gradientMax = options.gradientMax !== undefined ? options.gradientMax : 200;


        this.resistivityMin = options.resistivityMin !== undefined ? options.resistivityMin : 1000;
        this.resistivityMax = options.resistivityMax !== undefined ? options.resistivityMax : 20000;

        this.interpolationFactor = options.interpolationFactor || 10;
    }

    //Shared Utilities
    //Bilinear Interpolation
    interpolateAndSmooth(data)
    {
        const rows = data.length;
        const cols = data[0].length;

        const f = this.interpolationFactor;
        const newRows = (rows - 1) * f + 1;
        const newCols = (cols - 1) * f + 1;

        const out = Array.from({ length: newRows }, () => new Float32Array(newCols));

        for (let i = 0; i < newRows; i++)
        {
            for (let j = 0; j < newCols; j++)
            {
                const x = i / f, y = j / f;
                const x0 = Math.floor(x), x1 = Math.min(x0 + 1, rows -1);
                const y0 = Math.floor(y), y1 = Math.min(y0 + 1, cols - 1);
                const dx = x - x0, dy = y - y0;

                out[i][j] =
                    data[x0][y0] * (1 - dx) * (1 - dy) +
                    data[x1][y0] * dx       * (1 - dy) +
                    data[x0][y1] * (1 - dx) * dy       +
                    data[x1][y1] * dx       * dy;
            }
        }
        return out;
    }

    computeGradients(data)
    {
        const rows = data.length;
        const cols = data[0].length;
        const gradient = Array.from({ length: rows }, () => new Float32Array(cols));

        for (let i = 0; i < rows; i++)
        {
            for (let j = 0; j < cols; j++)
            {
                let gx, gy;

                if (j > 0 && j < cols - 1) gx = (data[i][j + 1] - data[i][j - 1]) / 2;
                else if (j === 0) gx =  data[i][j + 1] - data[i][j];
                else gx =  data[i][j]     - data[i][j - 1];

                if (i > 0 && i < rows - 1) gy = (data[i + 1][j] - data[i - 1][j]) / 2;
                else if (i === 0) gy =  data[i + 1][j] - data[i][j];
                else gy =  data[i][j]     - data[i - 1][j];

                gradient[i][j] = Math.sqrt(gx * gx + gy * gy);
            }
        }
        return gradient;
    }

    getContourColorscale()
    {
        return [
            [0,     'rgb(127, 0, 0)'],
            [0.125, 'rgb(255, 0, 0)'],
            [0.25,  'rgb(255, 127, 0)'],
            [0.375, 'rgb(255, 255, 0)'],
            [0.5,   'rgb(0, 255, 0)'],
            [0.625, 'rgb(0, 255, 255)'],
            [0.75,  'rgb(0, 127, 255)'],
            [0.875, 'rgb(0, 0, 255)'],
            [1,     'rgb(0, 0, 127)']
        ];
    }

    getGradientColorscale()
    {
        return [
            [0,     'rgb(0, 0, 127)'],
            [0.125, 'rgb(0, 0, 255)'],
            [0.25,  'rgb(0, 127, 255)'],
            [0.375, 'rgb(0, 255, 255)'],
            [0.5,   'rgb(0, 255, 0)'],
            [0.625, 'rgb(255, 255, 0)'],
            [0.75,  'rgb(255, 127, 0)'],
            [0.875, 'rgb(255, 0, 0)'],
            [1,     'rgb(127, 0, 0)']
        ];
    }

    /**
     * Canvas Rendering to Replace Plotly Dependency
     * renderToCanvas(data, canvas, options)
     */

    renderToCanvas(data, canvas, options = {})
    {
        if (!data || data.length < 2 || !data[0] || data[0].length < 2)
        {
            this._drawPlaceholder(canvas, 'Waiting for data...');
            return;
        }

        const ctx = canvas.getContext('2d');
        const W = canvas.width;
        const H = canvas.height;
        const colorscale = this.getContourColorscale();

        // Flip row order: pass 0 (first driven) is drawn at the bottom, last pass at top —
        // matching the orientation of the company's Plotly version (y-axis origin at bottom).
        const flippedData = [...data].reverse();

        const highRes = this.interpolateAndSmooth(flippedData);
        const hiRows = highRes.length;
        const hiCols = highRes[0].length;

        //Render Pixel by Pixel using Image Data for Performance
        const imageData = ctx.createImageData(W, H);
        const pix = imageData.data;

        for (let py = 0; py < H; py++)
        {
            const dataRow = Math.min(hiRows - 1, Math.floor(py * hiRows / H));
            const rowData = highRes[dataRow];

            for (let px = 0; px < W; px++)
            {
                const dataCol = Math.min(hiCols - 1, Math.floor(px * hiCols / W));
                const [r, g, b] = this._valueToRGB(rowData[dataCol], this.contourMin, this.contourMax, colorscale);
                const idx = (py * W + px) * 4;
                pix[idx] = r;
                pix[idx + 1] = g;
                pix[idx + 2] = b;
                pix[idx + 3] = 255;
            }
        }

        ctx.putImageData(imageData, 0, 0);

        if (options.showDataPoints)
        {
            this._drawDataLabels(ctx, data, flippedData, highRes, W, H);
        }

        //Title
        if (options.title)
        {
            ctx.save();
            ctx.font = 'bold 14px system-ui, sans-serif';
            ctx.fillStyle = 'rgba(255,255,255,0.85)';
            const padding = 6;
            const textWidth = ctx.measureText(options.title).width;
            ctx.fillRect(8,8, textWidth + padding * 2, 22);
            ctx.fillStyle = '#1e293b';
            ctx.fillText(options.title, 8 + padding, 24);
            ctx.restore();            
        }
    }


    //renderGradientToCanvas(data,canvas, options)
    //renders the gradient-magnitude map to a canvas element

    renderGradientToCanvas(data, canvas, options = {})
    {
     if (!data || data.length < 2) 
        {
            this._drawPlaceholder(canvas, 'Waiting for data…');
            return;
        }
 
        const ctx = canvas.getContext('2d');
        const W = canvas.width;
        const H = canvas.height;
        const colorscale = this.getGradientColorscale();
 
        const gradient = this.computeGradients(data);
        const flippedGrad = [...gradient].reverse();
        const highRes = this.interpolateAndSmooth(flippedGrad);
        const hiRows = highRes.length;
        const hiCols = highRes[0].length;
 
        const imageData = ctx.createImageData(W, H);
        const pix = imageData.data;
 
        for (let py = 0; py < H; py++) 
            {
                const dataRow = Math.min(hiRows - 1, Math.floor(py * hiRows / H));
                const rowData = highRes[dataRow];
 
                for (let px = 0; px < W; px++) 
                {
                    const dataCol = Math.min(hiCols - 1, Math.floor(px * hiCols / W));
                    const [r, g, b] = this._valueToRGB(rowData[dataCol], this.gradientMin, this.gradientMax, colorscale);
                    const idx = (py * W + px) * 4;
                    pix[idx]     = r;
                    pix[idx + 1] = g;
                    pix[idx + 2] = b;
                    pix[idx + 3] = 255;
                }
            }
 
        ctx.putImageData(imageData, 0, 0);
 
        if (options.title) 
        {
            ctx.save();
            ctx.font      = 'bold 14px system-ui, sans-serif';
            ctx.fillStyle = 'rgba(255,255,255,0.85)';
            const padding = 6;
            ctx.fillRect(8, 8, ctx.measureText(options.title).width + padding * 2, 22);
            ctx.fillStyle = '#1e293b';
            ctx.fillText(options.title, 8 + padding, 24);
            ctx.restore();
        }
    }
//--------------------------------------------------------------------------------------------------------------
    //Plotly API Preserved for desktop/PC use where Plotly is available
    //Pulled directly from provided code
        async generatePlotlyContour(data, containerDiv, options = {}) {
        if (typeof Plotly === 'undefined') {
            console.warn('Plotly is not loaded. Use renderToCanvas() instead.');
            return;
        }
 
        const tableName      = options.tableName   || 'Data';
        const showLabels     = options.showLabels   || false;
        const showDataPoints = options.showDataPoints || false;
 
        const flippedData = [...data].reverse();
        const rows = flippedData.length,  cols = flippedData[0].length;
        const width  = cols * this.gridSize,  height = rows * this.gridSize;
 
        const highResData = this.interpolateAndSmooth(flippedData);
        const highResRows = highResData.length, highResCols = highResData[0].length;
 
        const x = Array.from({ length: highResCols }, (_, i) => i * width  / (highResCols - 1));
        const y = Array.from({ length: highResRows }, (_, i) => i * height / (highResRows - 1));
 
        const tickInterval = 50;
        const tickStart = Math.floor(this.contourMin / tickInterval) * tickInterval;
        const tickEnd   = Math.ceil(this.contourMax  / tickInterval) * tickInterval;
        const tickVals  = [], tickTexts = [];
        for (let t = tickStart; t <= tickEnd; t += tickInterval) {
            tickVals.push(t); tickTexts.push(String(t));
        }
 
        const traces = [{
            type: 'heatmap', x, y,
            z: Array.from(highResData, r => Array.from(r)),
            colorscale: this.getContourColorscale(),
            colorbar: {
                title: { text: 'Potentials (w.r.t CSE mV)', side: 'right' },
                tickmode: 'array', tickvals: tickVals, ticktext: tickTexts
            },
            hovertemplate: 'X: %{x:.2f}<br>Y: %{y:.2f}<br>Value: %{z:.1f}<extra></extra>',
            zmin: this.contourMin, zmax: this.contourMax, zsmooth: 'best'
        }];
 
        if (showLabels) {
            traces.push({
                type: 'contour', x, y,
                z: Array.from(highResData, r => Array.from(r)),
                showscale: false,
                contours: {
                    start: this.contourMin, end: this.contourMax,
                    size: (this.contourMax - this.contourMin) / 20,
                    coloring: