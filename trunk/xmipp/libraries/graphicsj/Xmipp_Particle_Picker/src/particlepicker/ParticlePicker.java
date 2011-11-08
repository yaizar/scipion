package particlepicker;

import ij.CommandListener;
import ij.Executer;
import ij.ImageListener;
import ij.ImagePlus;
import ij.plugin.frame.Recorder;

import java.awt.Color;
import java.io.BufferedWriter;
import java.io.File;
import java.io.FileWriter;
import java.io.IOException;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.List;
import java.util.logging.FileHandler;
import java.util.logging.Level;
import java.util.logging.Logger;
import java.util.logging.SimpleFormatter;

import particlepicker.training.model.FamilyState;
import particlepicker.training.model.MicrographFamilyState;
import particlepicker.training.model.SupervisedParticlePicker;
import xmipp.MDLabel;
import xmipp.MetaData;
import xmipp.Program;
import xmipp.Particle;

public abstract class ParticlePicker
{

	private String familiesfile;
	private static Logger logger;
	private String outputdir = ".";
	private boolean changed;
	protected List<Family> families;
	private FamilyState mode;
	private String commands;
	private String command;

	public ParticlePicker(String outputdir, FamilyState mode)
	{
		commands = "";
		this.outputdir = outputdir;
		this.mode = mode;
		this.families = new ArrayList<Family>();
		this.familiesfile = getOutputPath("families.xmd");
		loadFamilies();

		Recorder.record = true;

		// detecting if a command is thrown by ImageJ
		Executer.addCommandListener(new CommandListener()
		{
			public String commandExecuting(String command)
			{
				ParticlePicker.this.command = command;
				return command;

			}
		});
		ImagePlus.addImageListener(new ImageListener()
		{

			@Override
			public void imageUpdated(ImagePlus arg0)
			{
				if (command != null)
				{
					String options = "";
					if (Recorder.getCommandOptions() != null)
						options = Recorder.getCommandOptions();
					addCommand(command, options);
					command = null;
				}
			}

			@Override
			public void imageOpened(ImagePlus arg0)
			{
				// TODO Auto-generated method stub

			}

			@Override
			public void imageClosed(ImagePlus arg0)
			{
				// TODO Auto-generated method stub

			}
		});
	}

	public void addCommand(String command, String options)
	{
		commands += String.format("run(\"%s\", \"%s\");\n", command, options);
		System.out.println(commands);
	}

	public String getCommands()
	{
		return commands;
	}

	public void loadMacros()
	{

	}

	public void setChanged(boolean changed)
	{
		this.changed = changed;
	}

	public boolean isChanged()
	{
		return changed;
	}

	public FamilyState getMode()
	{
		return mode;
	}

	public static Logger getLogger()
	{
		try
		{
			if (logger == null)
			{
				FileHandler fh = new FileHandler("PPicker.log", true);
				fh.setFormatter(new SimpleFormatter());
				logger = Logger.getLogger("PPickerLogger");
				logger.addHandler(fh);
			}
			return logger;
		}
		catch (Exception e)
		{
			// TODO Auto-generated catch block
			e.printStackTrace();
		}
		return null;
	}

	public String getOutputPath(String file)
	{
		return outputdir + File.separator + file;
	}

	public static String getXmippPath()
	{
		return Program.getXmippPath();
	}

	public static String getXmippPath(String relpath)
	{
		return getXmippPath() + File.separator + relpath;
	}

	public String getOutputDir()
	{
		return outputdir;
	}

	public List<Family> getFamilies()
	{
		return families;
	}

	public void persistFamilies()
	{
		long id;
		String file = familiesfile;
		try
		{
			MetaData md = new MetaData();
			for (Family f : families)
			{
				id = md.addObject();
				md.setValueString(MDLabel.MDL_PICKING_FAMILY, f.getName(), id);
				md.setValueInt(MDLabel.MDL_PICKING_COLOR, f.getColor().getRGB(), id);
				md.setValueInt(MDLabel.MDL_PICKING_PARTICLE_SIZE, f.getSize(), id);
				md.setValueString(MDLabel.MDL_PICKING_FAMILY_STATE, f.getStep().toString(), id);
			}
			md.write(file);
		}
		catch (Exception e)
		{
			getLogger().log(Level.SEVERE, e.getMessage(), e);
			throw new IllegalArgumentException(e.getMessage());
		}

	}

	public void loadFamilies()
	{
		families.clear();
		String filename = familiesfile;
		if (!new File(filename).exists())
		{
			families.add(Family.getDefaultFamily());
			return;
		}

		Family family;
		int rgb, size;
		FamilyState state;
		String name;
		try
		{
			MetaData md = new MetaData(filename);
			long[] ids = md.findObjects();
			for (long id : ids)
			{
				name = md.getValueString(MDLabel.MDL_PICKING_FAMILY, id);
				rgb = md.getValueInt(MDLabel.MDL_PICKING_COLOR, id);
				size = md.getValueInt(MDLabel.MDL_PICKING_PARTICLE_SIZE, id);
				state = FamilyState.valueOf(md.getValueString(MDLabel.MDL_PICKING_FAMILY_STATE, id));
				state = validateState(state);
				if (state == FamilyState.Supervised && this instanceof SupervisedParticlePicker)
					if (!new File(((SupervisedParticlePicker) this).getOTrainingFilename(name)).exists())
						throw new IllegalArgumentException(String.format("Training file does not exist. Family cannot be in %s mode", state));
				family = new Family(name, new Color(rgb), size, state, this);
				families.add(family);
			}
			if (families.size() == 0)
				throw new IllegalArgumentException(String.format("No families specified on %s", filename));
		}
		catch (Exception e)
		{
			getLogger().log(Level.SEVERE, e.getMessage(), e);
			throw new IllegalArgumentException(e.getMessage());
		}
	}

	public FamilyState validateState(FamilyState state)
	{

		if (mode == FamilyState.Review && state != FamilyState.Review)
		{
			setChanged(true);
			return FamilyState.Review;
		}
		if (mode == FamilyState.Manual && (state != FamilyState.Manual || state != FamilyState.Available))
			throw new IllegalArgumentException(String.format("Can not use %s mode on this data", mode));
		if (mode == FamilyState.Supervised && state == FamilyState.Review)
			throw new IllegalArgumentException(String.format("Can not use %s mode on this data", mode));
		return state;

	}

	public Family getFamily(String name)
	{
		for (Family f : getFamilies())
			if (f.getName().equalsIgnoreCase(name))
				return f;
		return null;
	}

	public boolean existsFamilyName(String name)
	{
		return getFamily(name) != null;
	}

	protected boolean containsBlock(String file, String block)
	{
		try
		{
			return Arrays.asList(MetaData.getBlocksInMetaDataFile(file)).contains(block);
		}
		catch (Exception e)
		{
			getLogger().log(Level.SEVERE, e.getMessage(), e);
			throw new IllegalArgumentException(e);
		}
	}

	public void removeFamily(Family family)
	{
		if (getManualParticlesNumber(family) > 0)// perhaps I have to check
													// automatic particles
			throw new IllegalArgumentException(Constants.getAssociatedDataMsg("family"));
		if (families.size() == 1)
			throw new IllegalArgumentException(Constants.getIllegalDeleteMsg("family"));
		families.remove(family);
	}

	public abstract void saveData();

	public abstract int getManualParticlesNumber(Family f);

	public void persistMacros()
	{
		String file = getOutputPath("macros.txt");
		try
		{
			BufferedWriter writer = new BufferedWriter(new FileWriter(file));
			writer.append(commands);
			writer.close();
		}
		catch (IOException e)
		{
			getLogger().log(Level.SEVERE, e.getMessage(), e);
			throw new IllegalArgumentException(e);
		}
	}

}
