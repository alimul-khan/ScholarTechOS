// Draws the fixed part of the home prayer page for the 480x480 screen: the Home Display app's prayer dashboard
// (ScreenRenderer.prayer, header, footer and the "Stepped" FrameStyle), laid out for a square screen.
// Live text (clock, dates, times, countdown, city) is drawn on top by the firmware (src/home.c), whose positions
// match the ones here.
// Run: tools/home_art/make.sh   ->  build/home_bg.png (preview) and src/gen/home_bg.c (RGB565 for LVGL)
import java.awt.*;
import java.awt.font.TextAttribute;
import java.awt.geom.*;
import java.awt.image.BufferedImage;
import java.io.*;
import java.util.Map;
import javax.imageio.ImageIO;

public class HomeArt {
    static final int W = 480, H = 480, S = 2;          // drawn at 2x, then scaled down for smooth edges
    static final Color GREEN = new Color(6, 53, 43), GOLD = new Color(193, 151, 73), PALE = new Color(246, 241, 225),
        INK = new Color(4, 51, 42), GOLD_TEXT = new Color(236, 207, 153);
    // Layout shared with src/home.c.
    static final int CART_Y = 186, CART_H = 94, CARD_Y = 290, CARD_H = 54, FOOT = 368;
    static final int[] CARD_X = {10, 165, 320}; static final int CARD_W = 150;
    static Graphics2D g;
    static Font bold, regular;

    public static void main(String[] a) throws Exception {
        String dir = a[0], art = a[1], build = a[2], gen = a[3];
        bold = Font.createFont(Font.TRUETYPE_FONT, new File(dir, "Serif-Bold.ttf"));
        regular = Font.createFont(Font.TRUETYPE_FONT, new File(dir, "Serif-Regular.ttf"));
        BufferedImage photo = ImageIO.read(new File(art));
        BufferedImage prayer = render(W, H, false, () -> { background(photo); header(INK); cartouche(); cards(); footer(); corners(); });
        BufferedImage weather = render(W, H, false, () -> { weatherBackdrop(); header(new Color(236, 214, 160)); weatherPanels(); footer(); corners(); });
        ImageIO.write(prayer, "png", new File(build, "home_bg.png"));
        ImageIO.write(weather, "png", new File(build, "home_wx.png"));
        // PNG in the firmware (small); src/home.c decodes them once at start-up.
        StringBuilder c = new StringBuilder();
        c.append(png(prayer, "home_bg")).append(png(weather, "home_wx"));
        // Weather icons (ScreenRenderer.weatherIcon) for: clear, partly cloudy, cloudy/fog, rain, snow.
        int[] codes = {0, 2, 45, 61, 71};
        for (int i = 0; i < 5; i++) {
            final int code = codes[i];
            c.append(png(render(120, 120, true, () -> weatherIcon(60, 66, .56f, code)), "wx_l" + i));
            c.append(png(render(48, 48, true, () -> weatherIcon(24, 26, .22f, code)), "wx_s" + i));
        }
        c.append("const uint8_t *const wx_png_l[5] = {wx_l0_png, wx_l1_png, wx_l2_png, wx_l3_png, wx_l4_png};\n");
        c.append("const unsigned wx_png_l_len[5] = {sizeof(wx_l0_png), sizeof(wx_l1_png), sizeof(wx_l2_png), sizeof(wx_l3_png), sizeof(wx_l4_png)};\n");
        c.append("const uint8_t *const wx_png_s[5] = {wx_s0_png, wx_s1_png, wx_s2_png, wx_s3_png, wx_s4_png};\n");
        c.append("const unsigned wx_png_s_len[5] = {sizeof(wx_s0_png), sizeof(wx_s1_png), sizeof(wx_s2_png), sizeof(wx_s3_png), sizeof(wx_s4_png)};\n");
        write(new File(gen, "home_art.c"), c.toString());
    }

    static String png(BufferedImage img, String name) throws IOException {
        ByteArrayOutputStream bytes = new ByteArrayOutputStream();
        ImageIO.write(img, "png", bytes);
        StringBuilder b = new StringBuilder("const uint8_t " + name + "_png[] = {");
        byte[] d = bytes.toByteArray();
        for (int i = 0; i < d.length; i++) { if (i % 32 == 0) b.append('\n'); b.append(d[i] & 255).append(','); }
        b.append("\n};\nconst unsigned ").append(name).append("_png_len = sizeof(").append(name).append("_png);\n");
        return b.toString();
    }

    /** Draws at 2x into a w x h image (transparent when alpha), then scales down for smooth edges. */
    static BufferedImage render(int w, int h, boolean alpha, Runnable draw) {
        int type = alpha ? BufferedImage.TYPE_INT_ARGB : BufferedImage.TYPE_INT_RGB;
        BufferedImage big = new BufferedImage(w * S, h * S, type);
        g = big.createGraphics();
        g.setRenderingHint(RenderingHints.KEY_ANTIALIASING, RenderingHints.VALUE_ANTIALIAS_ON);
        g.setRenderingHint(RenderingHints.KEY_TEXT_ANTIALIASING, RenderingHints.VALUE_TEXT_ANTIALIAS_ON);
        g.setRenderingHint(RenderingHints.KEY_FRACTIONALMETRICS, RenderingHints.VALUE_FRACTIONALMETRICS_ON);
        g.setRenderingHint(RenderingHints.KEY_INTERPOLATION, RenderingHints.VALUE_INTERPOLATION_BICUBIC);
        g.setRenderingHint(RenderingHints.KEY_STROKE_CONTROL, RenderingHints.VALUE_STROKE_PURE);
        g.scale(S, S);
        draw.run();
        g.dispose();
        BufferedImage out = new BufferedImage(w, h, type);
        Graphics2D o = out.createGraphics();
        o.setComposite(AlphaComposite.Src);
        o.drawImage(big.getScaledInstance(w, h, Image.SCALE_AREA_AVERAGING), 0, 0, null);
        o.dispose();
        return out;
    }

    // ---- helpers -------------------------------------------------------------------------------------------
    static Color rgba(int r, int gg, int b, int al) { return new Color(r, gg, b, al); }
    static void fill(Shape s, Paint p) { g.setPaint(p); g.fill(s); }
    static void stroke(Shape s, Paint p, float w) { g.setPaint(p); g.setStroke(new BasicStroke(w, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND)); g.draw(s); }
    static void line(float x, float y, float xx, float yy, Color col, float w) { stroke(new Line2D.Float(x, y, xx, yy), col, w); }
    static LinearGradientPaint lin(float x0, float y0, float x1, float y1, Color... cs) {
        float[] f = new float[cs.length];
        for (int i = 0; i < cs.length; i++) f[i] = cs.length == 1 ? 0 : i / (float) (cs.length - 1);
        return new LinearGradientPaint(x0, y0, x1, y1, f, cs);
    }
    static Font font(Font base, float size, float tracking) {
        return base.deriveFont(Map.of(TextAttribute.SIZE, size, TextAttribute.TRACKING, tracking));
    }
    static void centered(String s, float cx, float y, Font f, Color col) {
        g.setFont(f); g.setPaint(col);
        float w = (float) f.getStringBounds(s, g.getFontRenderContext()).getWidth();
        g.drawString(s, cx - w / 2, y);
    }
    static void text(String s, float x, float y, Font f, Color col) { g.setFont(f); g.setPaint(col); g.drawString(s, x, y); }
    static void diamond(float x, float y, float r, Color col) {
        Path2D q = new Path2D.Float(); q.moveTo(x, y - r); q.lineTo(x + r, y); q.lineTo(x, y + r); q.lineTo(x - r, y); q.closePath(); fill(q, col);
    }

    // ---- background: the app's prayer artwork with its ivory wash ------------------------------------------
    static void background(BufferedImage bg) {
        // The right part of the landscape (the mosque and the sun), as a square.
        int side = bg.getHeight(), x0 = bg.getWidth() - side;
        g.drawImage(bg, 0, 0, W, H, x0, 0, x0 + side, side, null);
        Color c = new Color(248, 244, 230);
        g.setPaint(new LinearGradientPaint(0, 0, W, 0, new float[]{0, .5f, .78f, 1},
            new Color[]{rgba(248, 244, 230, 245), rgba(248, 244, 230, 222), rgba(248, 244, 230, 150), rgba(248, 244, 230, 60)}));
        g.fillRect(0, 0, W, FOOT + 10);
        // A softer wash behind the clock and dates, so they read over the sky.
        g.setPaint(new LinearGradientPaint(0, 40, 0, 180, new float[]{0, .5f, 1},
            new Color[]{rgba(248, 244, 230, 0), rgba(248, 244, 230, 110), rgba(248, 244, 230, 0)}));
        g.fillRect(0, 40, W, 140);
    }

    // ---- header: HOME • FAITH • DAILY, the place pin and Wi-Fi ---------------------------------------------
    static void header(Color ink) {
        line(100, 15, 138, 15, GOLD, 1); line(342, 15, 380, 15, GOLD, 1);
        centered("HOME • FAITH • DAILY", 240, 19, font(bold, 9.5f, .16f), ink);
        diamond(240, 29, 3.5f, GOLD);
        pin(80, 52);     // the gear (settings) sits top right, drawn by the firmware
    }
    static void pin(float x, float y) {
        Path2D q = new Path2D.Float(); q.moveTo(x - 5, y - 6.5); q.lineTo(x, y + 3); q.lineTo(x + 5, y - 6.5); q.closePath();
        fill(q, GOLD); fill(new Ellipse2D.Float(x - 5.6f, y - 15.6f, 11.2f, 11.2f), GOLD);
        fill(new Ellipse2D.Float(x - 2, y - 12, 4, 4), PALE);
    }
    static void wifi(float x, float y, float s, Color col) {
        for (int i = 1; i <= 3; i++) {
            float r = s * i * .3f;
            g.setPaint(col); g.setStroke(new BasicStroke(s * .13f, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND));
            g.draw(new Arc2D.Float(x - r, y + s * .35f - r, 2 * r, 2 * r, 45, 90, Arc2D.OPEN));
        }
        float d = s * .14f; fill(new Ellipse2D.Float(x - d / 2, y + s * .35f - d / 2, d, d), col);
    }

    // ---- the NEXT AZAN cartouche -----------------------------------------------------------------------------
    static Path2D cartPath() {
        // ScreenRenderer.prayer's cartouche (x 28..846, y 286..453), fitted to this screen.
        Path2D c = new Path2D.Float();
        c.moveTo(60, 286); c.lineTo(812, 286); c.curveTo(822, 286, 830, 296, 838, 310); c.curveTo(846, 322, 846, 325, 838, 335);
        c.lineTo(819, 367); c.curveTo(814, 376, 814, 378, 819, 388); c.lineTo(838, 422); c.curveTo(846, 432, 846, 440, 836, 446);
        c.curveTo(828, 452, 820, 453, 810, 453); c.lineTo(60, 453); c.curveTo(50, 453, 40, 448, 34, 440); c.curveTo(28, 430, 28, 424, 34, 412);
        c.lineTo(60, 372); c.lineTo(34, 332); c.curveTo(28, 320, 30, 312, 36, 304); c.curveTo(42, 294, 50, 286, 60, 286); c.closePath();
        AffineTransform t = new AffineTransform();
        t.translate(8, CART_Y); t.scale(464.0 / 818, CART_H / 167.0); t.translate(-28, -286);
        return (Path2D) c.createTransformedShape(t);
    }
    static void cartouche() {
        Path2D c = cartPath();
        fill(c, lin(0, CART_Y, 0, CART_Y + CART_H, new Color(9, 68, 54), new Color(4, 45, 37)));
        stroke(c, GOLD, 1.6f);
        Rectangle2D b = c.getBounds2D();
        AffineTransform in = new AffineTransform();
        in.translate(b.getCenterX(), b.getCenterY()); in.scale(.975, .88); in.translate(-b.getCenterX(), -b.getCenterY());
        stroke(in.createTransformedShape(c), GOLD, .6f);
        minaret(70, CART_Y + CART_H - 16, 58, GOLD, new Color(6, 56, 45));
        line(178, CART_Y + 16, 216, CART_Y + 16, GOLD, .8f); line(316, CART_Y + 16, 354, CART_Y + 16, GOLD, .8f);
        centered("NEXT AZAN", 266, CART_Y + 20, font(regular, 10.5f, .2f), GOLD_TEXT);
    }
    /** ScreenRenderer.domeIcon: mosque with dome and spire; (cx,by) is the bottom centre. */
    static void dome(float cx, float by, float s, Color col, Color cut) {
        fill(new Rectangle2D.Float(cx - .5f * s, by - .3f * s, s, .3f * s), col);
        fill(new Arc2D.Float(cx - .4f * s, by - .7f * s, .8f * s, .8f * s, 0, 180, Arc2D.PIE), col);
        fill(new Rectangle2D.Float(cx - .025f * s, by - .98f * s, .05f * s, .3f * s), col);
        fill(new Ellipse2D.Float(cx - .05f * s, by - 1.03f * s, .1f * s, .1f * s), col);
        if (cut != null) for (int i = -1; i <= 1; i++) {
            float ax = cx + i * .3f * s;
            fill(new Rectangle2D.Float(ax - .07f * s, by - .17f * s, .14f * s, .17f * s), cut);
            fill(new Arc2D.Float(ax - .07f * s, by - .24f * s, .14f * s, .14f * s, 0, 180, Arc2D.PIE), cut);
        }
    }
    static void minaret(float cx, float by, float s, Color col, Color cut) {
        dome(cx + .1f * s, by, s * .78f, col, cut);
        float x = cx - .55f * s;
        fill(new Rectangle2D.Float(x - .06f * s, by - .62f * s, .12f * s, .62f * s), col);
        fill(new Rectangle2D.Float(x - .1f * s, by - .64f * s, .2f * s, .06f * s), col);
        Path2D q = new Path2D.Float(); q.moveTo(x - .08f * s, by - .64f * s); q.lineTo(x, by - .86f * s); q.lineTo(x + .08f * s, by - .64f * s); q.closePath(); fill(q, col);
    }

    // ---- sunrise, solar noon, sunset ---------------------------------------------------------------------------
    static void cards() {
        String[] names = {"SUNRISE", "SOLAR NOON", "SUNSET"};
        for (int i = 0; i < 3; i++) {
            float x = CARD_X[i], y = CARD_Y;
            RoundRectangle2D r = new RoundRectangle2D.Float(x, y, CARD_W, CARD_H, 9, 9);
            fill(r, rgba(250, 247, 236, 238)); stroke(r, GOLD, 1.1f);
            solar(x + 23, y + 29, 12, i, GOLD);
            text(names[i], x + 42, y + 18, font(bold, 8.5f, .16f), INK);
        }
    }
    /** ScreenRenderer.solarIcon: 0 sunrise, 1 noon (full sun), 2 sunset. */
    static void solar(float x, float y, float r, int type, Color col) {
        g.setPaint(col); g.setStroke(new BasicStroke(1.2f, BasicStroke.CAP_ROUND, BasicStroke.JOIN_ROUND));
        if (type == 1) g.draw(new Ellipse2D.Float(x - r * .55f, y - r * .55f, r * 1.1f, r * 1.1f));
        else g.draw(new Arc2D.Float(x - r * .65f, y - r * .65f, r * 1.3f, r * 1.3f, 0, 180, Arc2D.OPEN));
        for (int i = 0; i < (type == 1 ? 8 : 5); i++) {
            double an = (type == 1 ? i * 45 : 180 + i * 45) * Math.PI / 180;
            float a1 = r * .83f, a2 = r * 1.12f;
            g.draw(new Line2D.Float(x + (float) Math.cos(an) * a1, y + (float) Math.sin(an) * a1, x + (float) Math.cos(an) * a2, y + (float) Math.sin(an) * a2));
        }
        if (type != 1) { line(x - r, y + 3, x + r, y + 3, col, 1.1f); line(x - r * .7f, y + 7, x + r * .7f, y + 7, col, 1.1f); }
    }

    // ---- the weather page: dark emerald with a warm glow and a mosque silhouette (ScreenRenderer.weatherBackdrop) ----
    static void weatherBackdrop() {
        g.setPaint(lin(0, 0, W, 0, new Color(4, 42, 34), new Color(24, 84, 66)));
        g.fillRect(0, 0, W, H);
        g.setPaint(new RadialGradientPaint(400, 300, 230, new float[]{0, 1}, new Color[]{rgba(236, 176, 84, 120), rgba(236, 176, 84, 0)}));
        g.fillRect(0, 0, W, H);
        Color sil = new Color(10, 48, 40), body = new Color(16, 66, 54);
        fill(new Rectangle2D.Float(0, 352, W, 30), new Color(12, 58, 48));
        for (int i = 0; i < 9; i++) { float tx = 300 + i * 9, ty = 352 + (i * 7 % 5) * 2; fill(new Ellipse2D.Float(tx - 9, ty - 14, 18, 18), sil); }
        float bx = 425;
        fill(new Rectangle2D.Float(bx - 34, 334, 72, 20), body);
        fill(new Arc2D.Float(bx - 22, 304, 40, 40, 0, 180, Arc2D.PIE), body);
        fill(new Rectangle2D.Float(bx - 3, 296, 2, 9), body);
        for (int t = 0; t < 2; t++) {
            float mx = t == 0 ? 386 : 466, top = t == 0 ? 312 : 290;
            fill(new Rectangle2D.Float(mx - 3, top, 6, 354 - top), body);
            fill(new Rectangle2D.Float(mx - 4.5f, top + 7, 9, 3), body);
            Path2D q = new Path2D.Float(); q.moveTo(mx - 4, top); q.lineTo(mx, top - 11); q.lineTo(mx + 4, top); q.closePath(); fill(q, body);
        }
        for (int i = 0; i < 5; i++) fill(new Rectangle2D.Float(bx - 28 + i * 13, 340, 3, 7), rgba(255, 214, 110, 150));
    }
    static void weatherPanels() {
        for (int i = 1; i < 4; i++) line(i * 120, 218, i * 120, 256, GOLD, .8f);
        for (int i = 0; i < 4; i++) {
            RoundRectangle2D r = new RoundRectangle2D.Float(12 + i * 116, 266, 108, 84, 12, 12);
            fill(r, rgba(255, 255, 255, 40)); stroke(r, rgba(236, 206, 140, 110), .9f);
        }
    }
    static void star(float x, float y, float r, Color col, float w) {
        Path2D q = new Path2D.Float();
        for (int i = 0; i < 16; i++) {
            double an = -Math.PI / 2 + i * Math.PI / 8; float rr = i % 2 == 0 ? r : r * .48f;
            float xx = x + (float) Math.cos(an) * rr, yy = y + (float) Math.sin(an) * rr;
            if (i == 0) q.moveTo(xx, yy); else q.lineTo(xx, yy);
        }
        q.closePath(); stroke(q, col, w);
    }
    /** ScreenRenderer.weatherIcon, centred on (x,y) at scale s. */
    static void weatherIcon(float x, float y, float s, int code) {
        AffineTransform keep = g.getTransform();
        g.translate(x, y); g.scale(s, s);
        if (code <= 3) {
            g.setPaint(new RadialGradientPaint(0, -14, 95, new float[]{0, 1}, new Color[]{rgba(255, 214, 110, 150), rgba(255, 214, 110, 0)}));
            g.fill(new Ellipse2D.Float(-95, -109, 190, 190));
            for (int i = 0; i < 16; i++) {
                double an = i * Math.PI / 8; float o = i % 2 == 0 ? 74 : 66;
                line((float) Math.cos(an) * 56, (float) Math.sin(an) * 56 - 14, (float) Math.cos(an) * o, (float) Math.sin(an) * o - 14, rgba(250, 214, 120, 170), 3);
            }
            fill(new Ellipse2D.Float(-48, -62, 96, 96), lin(0, -62, 0, 34, new Color(255, 220, 120), new Color(240, 170, 50)));
        }
        if (code != 0) {
            Area cl = new Area(new Ellipse2D.Float(-63, -9, 70, 70));
            cl.add(new Area(new Ellipse2D.Float(-33, -33, 90, 90)));
            cl.add(new Area(new Ellipse2D.Float(18, 6, 56, 56)));
            cl.add(new Area(new RoundRectangle2D.Float(-65, 27, 140, 35, 36, 36)));
            cl.add(new Area(new Rectangle2D.Float(-30, 30, 78, 32)));
            fill(cl, lin(0, -33, 0, 62, new Color(252, 252, 246), new Color(205, 210, 204)));
        }
        if (code >= 51 && code <= 67 || code >= 80 && code <= 82 || code >= 95)
            for (int i = 0; i < 4; i++) line(-40 + i * 28, 76, -46 + i * 28, 92, new Color(157, 201, 206), 5);
        if (code >= 71 && code <= 77 || code == 85 || code == 86)
            for (int i = 0; i < 4; i++) star(-40 + i * 28, 82, 7, Color.WHITE, 2.5f);
        g.setTransform(keep);
    }

    // ---- the prayer bar ------------------------------------------------------------------------------------------
    static void footer() {
        int top = FOOT;
        Path2D f = new Path2D.Float();
        f.moveTo(0, top + 8); f.curveTo(8, top + 1, 18, top, 32, top); f.lineTo(W - 32, top); f.curveTo(W - 18, top, W - 8, top + 1, W, top + 8);
        f.lineTo(W, H); f.lineTo(0, H); f.closePath();
        fill(f, lin(0, top, 0, H, new Color(9, 68, 54), new Color(4, 45, 37)));
        stroke(f, GOLD, 1.5f);
        line(32, top + 4, W - 32, top + 4, rgba(193, 151, 73, 110), .6f);
        String[] names = {"FAJR", "DHUHR", "ASR", "MAGHRIB", "ISHA"};
        Font nf = font(regular, 9.5f, .16f);
        for (int i = 0; i < 5; i++) {
            float cx = 48 + i * 96;
            dome(cx, top + 31, 19, GOLD, new Color(7, 60, 48));
            centered(names[i], cx, top + 47, nf, GOLD_TEXT);
            if (i < 4) line(cx + 48, top + 14, cx + 48, H - 12, GOLD, .6f);
        }
    }

    // ---- FrameStyles.Stepped ----------------------------------------------------------------------------------------
    static Path2D stepped(float sx, float sy, float k) {
        sx *= k; sy *= k; float bx = .64f, by = .26f, cx = .64f, cy = .64f, dx = .26f, dy = .64f, rr = .06f;
        Path2D q = new Path2D.Float(); q.moveTo(0, 0); q.lineTo(sx, 0);
        q.curveTo(sx * (1 - .08f), sy * .095f, sx * (bx + .115f), sy * (by - .047f), sx * bx, sy * by);
        q.lineTo(sx * cx, sy * (cy - rr)); q.quadTo(sx * cx, sy * cy, sx * (cx - rr), sy * cy); q.lineTo(sx * dx, sy * dy);
        q.curveTo(sx * (dx - .047f), sy * (dy + .115f), sx * .095f, sy * (1 - .08f), 0, sy); q.closePath(); return q;
    }
    static void girih(float size) {
        float d = 30, t = d / 2, in = d * .383f;
        for (int pass = 0; pass < 2; pass++) {
            g.setPaint(pass == 0 ? rgba(0, 20, 14, 150) : new Color(206, 165, 86));
            g.setStroke(new BasicStroke(pass == 0 ? 2.4f : 1.25f, BasicStroke.CAP_BUTT, BasicStroke.JOIN_MITER));
            for (int layer = 0; layer < 2; layer++)
                for (float y = layer * d / 2; y <= size + d; y += d)
                    for (float x = layer * d / 2; x <= size + d; x += d) {
                        Path2D q = new Path2D.Float();
                        for (int i = 0; i < 16; i++) {
                            double an = i * Math.PI / 8 + layer * Math.PI / 8;
                            float rr = (i % 2 == 0 ? t : in) * (layer == 0 ? 1 : .5f);
                            float xx = x + (float) Math.cos(an) * rr, yy = y + (float) Math.sin(an) * rr;
                            if (i == 0) q.moveTo(xx, yy); else q.lineTo(xx, yy);
                        }
                        q.closePath(); g.draw(q);
                    }
        }
    }
    static void corners() {
        for (int k = 0; k < 4; k++) {
            AffineTransform keep = g.getTransform();
            if (k % 2 == 1) { g.translate(W, 0); g.scale(-1, 1); }
            if (k >= 2) { g.translate(0, H); g.scale(1, -1); }
            float sx = k >= 2 ? 30 : 88, sy = k >= 2 ? 50 : 88;
            Path2D outer = stepped(sx, sy, 1), inner = stepped(sx, sy, .9f);
            fill(outer, lin(0, 0, sx, sy, new Color(9, 66, 52), new Color(3, 38, 31)));
            Shape clip = g.getClip(); g.clip(inner); girih(Math.max(sx, sy)); g.setClip(clip);
            stroke(outer, new Color(150, 106, 40), 3.4f);
            stroke(outer, new LinearGradientPaint(0, 0, sx, sy, new float[]{0, .33f, .66f, 1},
                new Color[]{new Color(250, 226, 160), new Color(214, 170, 90), new Color(246, 214, 140), new Color(190, 140, 60)}), 2f);
            stroke(inner, new Color(222, 184, 110), .9f);
            g.setTransform(keep);
        }
    }

    // ---- images for LVGL: RGB565, or ARGB8888 (B, G, R, A in memory) with transparency ----------------------------------
    static String image(BufferedImage img, String name, boolean alpha) {
        int w = img.getWidth(), h = img.getHeight(), bpp = alpha ? 4 : 2;
        StringBuilder b = new StringBuilder("static const uint8_t " + name + "_data[] = {\n");
        for (int y = 0; y < h; y++) {
            for (int x = 0; x < w; x++) {
                int p = img.getRGB(x, y), al = (p >>> 24) & 255, r = (p >> 16) & 255, gg = (p >> 8) & 255, bl = p & 255;
                if (alpha) b.append(bl).append(',').append(gg).append(',').append(r).append(',').append(al).append(',');
                else { int v = ((r >> 3) << 11) | ((gg >> 2) << 5) | (bl >> 3); b.append(v & 255).append(',').append(v >> 8).append(','); }
            }
            b.append('\n');
        }
        b.append("};\nconst lv_image_dsc_t ").append(name).append(" = {\n    .header = {.magic = LV_IMAGE_HEADER_MAGIC, .cf = ")
         .append(alpha ? "LV_COLOR_FORMAT_ARGB8888" : "LV_COLOR_FORMAT_RGB565").append(", .w = ").append(w).append(", .h = ").append(h)
         .append(", .stride = ").append(w * bpp).append("},\n    .data_size = sizeof(").append(name).append("_data),\n    .data = ")
         .append(name).append("_data,\n};\n");
        return b.toString();
    }
    static void write(File f, String body) throws IOException {
        try (PrintWriter w = new PrintWriter(new BufferedWriter(new FileWriter(f)))) {
            w.println("/* Made by tools/home_art (HomeArt.java): artwork for the home pages. Do not edit. */");
            w.println("#include <stdint.h>");
            w.print(body);
        }
    }
}
