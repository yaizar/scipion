package browser.imageitems;

import ij.ImagePlus;
import ij.ImageStack;
import ij.io.FileInfo;
import ij.process.FloatProcessor;
import ij.util.Tools;
import java.util.Vector;
import xmipp.ImageDouble;
import xmipp.Projection;

/*
 * To change this template, choose Tools | Templates
 * and open the template in the editor.
 */
/**
 *
 * @author Juanjo Vega
 */
public class ImageConverter {

    public static ImagePlus convertToImagej(ImageDouble image, String path) {
        if (image.isPSD()) {
            image.convertPSD();
        }

        int w = image.getXsize();
        int h = image.getYsize();
        int d = image.getZsize();
        long n = image.getNsize();

        ImagePlus ip = convertToImagej(image.getData(), w, h, d, n, path);
        ip.setFileInfo(new FileInfo());

        return ip;
    }

    public static ImagePlus convertToImagej(Projection projection, String path) {

        int w = projection.getXsize();
        int h = projection.getYsize();
        int d = projection.getZsize();

        return convertToImagej(projection.getData(), w, h, d, 1, path);
    }

    private static ImagePlus convertToImagej(double array[], int w, int h, int d, long n, String title) {
        int sliceSize = w * h;
        int imageSize = sliceSize * d;
        double out[] = new double[sliceSize];
        ImageStack is = new ImageStack(w, h);

        for (int i = 0; i < n; i++) {
            int offset = i * imageSize;
            for (int j = 0; j < d; j++) {
                System.arraycopy(array, offset + j * sliceSize, out, 0, sliceSize);

                FloatProcessor processor = new FloatProcessor(w, h, out);
                is.addSlice(String.valueOf(i), processor);
            }
        }

        return new ImagePlus(title, is);
    }

    public static ImageDouble convertToXmipp(ImagePlus ip) {
        ImageDouble image = new ImageDouble();

        int w = ip.getWidth();
        int h = ip.getHeight();
        int d = ip.getStackSize();

        double data[] = new double[w * h * d];
        for (int i = 0; i < d; i++) {
            float slice[] = (float[]) ip.getStack().getProcessor(i + 1).getPixels();
            System.arraycopy(Tools.toDouble(slice), 0, data, i * w * h, w * h);
        }
        try {
            image.setData(w, h, d, data);
        } catch (Exception ex) {
            ex.printStackTrace();
        }

        return image;
    }

    public static ImagePlus toImagePlus(Vector<TableImageItem> items, String title) {
        TableImageItem item = items.elementAt(0);

        ImageStack is = new ImageStack(item.getWidth(), item.getHeight());

        for (int i = 0; i < items.size(); i++) {
            is.addSlice(String.valueOf(i), items.elementAt(i).getPreview().getProcessor());
        }

        return new ImagePlus(title, is);
    }
}
