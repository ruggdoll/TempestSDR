/*******************************************************************************
 * Copyright (c) 2014 Martin Marinov.
 * All rights reserved. This program and the accompanying materials
 * are made available under the terms of the GNU Public License v3.0
 * which accompanies this distribution, and is available at
 * http://www.gnu.org/licenses/gpl.html
 *
 * Contributors:
 *     Martin Marinov - initial API and implementation
 ******************************************************************************/
package martin.tempest.sources;

import java.awt.Container;
import java.awt.event.ActionEvent;
import java.awt.event.ActionListener;

import javax.swing.JLabel;
import javax.swing.JTextField;

/**
 * This plugin connects to USRP devices via the UHD driver.
 *
 * @author Martin Marinov
 *
 */
public class TSDRUHDSource extends TSDRSource {

	public TSDRUHDSource() {
		super("USRP (via UHD)", "TSDRPlugin_UHD", false);
	}

	private static ParsedParams parseParams(String params) {
		ParsedParams p = new ParsedParams();
		if (params == null || params.trim().isEmpty()) return p;

		String[] tokens = params.trim().split("\\s+");
		for (int i = 0; i < tokens.length - 1; i++) {
			String key = tokens[i];
			String value = tokens[i + 1];

			if (key.equals("--rate")) {
				try { p.rate = value; } catch (Exception e) {}
				i++;
			} else if (key.equals("--bw")) {
				try { p.bw = value; } catch (Exception e) {}
				i++;
			} else if (key.equals("--args")) {
				p.args = value;
				i++;
			} else if (key.equals("--ant")) {
				p.ant = value;
				i++;
			} else if (key.equals("--subdev")) {
				p.subdev = value;
				i++;
			}
		}
		return p;
	}

	private static String buildParams(String args, String rate, String bw, String ant, String subdev) {
		StringBuilder sb = new StringBuilder();
		if (args != null && !args.trim().isEmpty())
			sb.append("--args ").append(args.trim());
		if (rate != null && !rate.trim().isEmpty()) {
			if (sb.length() > 0) sb.append(" ");
			sb.append("--rate ").append(rate.trim());
		}
		if (bw != null && !bw.trim().isEmpty()) {
			if (sb.length() > 0) sb.append(" ");
			sb.append("--bw ").append(bw.trim());
		}
		if (ant != null && !ant.trim().isEmpty()) {
			if (sb.length() > 0) sb.append(" ");
			sb.append("--ant ").append(ant.trim());
		}
		if (subdev != null && !subdev.trim().isEmpty()) {
			if (sb.length() > 0) sb.append(" ");
			sb.append("--subdev ").append(subdev.trim());
		}
		return sb.toString();
	}

	@Override
	public boolean populateGUI(final Container cont, final String defaultprefs, final ActionListenerRegistrator okbutton) {
		final ParsedParams parsed = parseParams(defaultprefs);

		final JLabel lblArgs = new JLabel("Device args:");
		cont.add(lblArgs);
		lblArgs.setBounds(12, 8, 85, 24);

		final JTextField txtArgs = new JTextField(parsed.args);
		cont.add(txtArgs);
		txtArgs.setBounds(100, 8, 160, 24);

		final JLabel lblRate = new JLabel("Rate (Hz):");
		cont.add(lblRate);
		lblRate.setBounds(270, 8, 70, 24);

		final JTextField txtRate = new JTextField(parsed.rate);
		cont.add(txtRate);
		txtRate.setBounds(345, 8, 100, 24);

		final JLabel lblBw = new JLabel("BW (Hz):");
		cont.add(lblBw);
		lblBw.setBounds(12, 40, 85, 24);

		final JTextField txtBw = new JTextField(parsed.bw);
		cont.add(txtBw);
		txtBw.setBounds(100, 40, 160, 24);

		final JLabel lblAnt = new JLabel("Antenna:");
		cont.add(lblAnt);
		lblAnt.setBounds(270, 40, 70, 24);

		final JTextField txtAnt = new JTextField(parsed.ant);
		cont.add(txtAnt);
		txtAnt.setBounds(345, 40, 100, 24);

		okbutton.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent arg0) {
				setParams(buildParams(
					txtArgs.getText(),
					txtRate.getText(),
					txtBw.getText(),
					txtAnt.getText(),
					""
				));
			}
		});

		return true;
	}

	private static class ParsedParams {
		String args = "";
		String rate = "25000000";
		String bw = "";
		String ant = "";
		String subdev = "";
	}
}
