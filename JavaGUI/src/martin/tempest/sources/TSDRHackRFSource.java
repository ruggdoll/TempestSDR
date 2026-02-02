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

import javax.swing.DefaultComboBoxModel;
import javax.swing.JCheckBox;
import javax.swing.JComboBox;
import javax.swing.JLabel;

/**
 * This is a source that works with HackRF devices, for more information see <a href="https://greatscottgadgets.com/hackrf/">greatscottgadgets.com</a>
 *
 * @author Martin Marinov
 *
 */
public class TSDRHackRFSource extends TSDRSource {

	private static final long[] SAMPLE_RATES = {
		2000000, 4000000, 8000000, 10000000, 12000000, 16000000, 20000000
	};
	private static final String[] SAMPLE_RATE_LABELS = {
		"2 MHz", "4 MHz", "8 MHz", "10 MHz", "12 MHz", "16 MHz", "20 MHz"
	};
	private static final int DEFAULT_SAMPLE_RATE_INDEX = 6; // 20 MHz

	private static final long[] BANDWIDTHS = {
		0, 1750000, 2500000, 3500000, 5000000, 5500000, 6000000,
		7000000, 8000000, 9000000, 10000000, 12000000, 14000000,
		15000000, 20000000, 24000000, 28000000
	};
	private static final String[] BANDWIDTH_LABELS = {
		"Auto (= sample rate)", "1.75 MHz", "2.5 MHz", "3.5 MHz", "5 MHz", "5.5 MHz", "6 MHz",
		"7 MHz", "8 MHz", "9 MHz", "10 MHz", "12 MHz", "14 MHz",
		"15 MHz", "20 MHz", "24 MHz", "28 MHz"
	};
	private static final int DEFAULT_BANDWIDTH_INDEX = 0; // Auto

	public TSDRHackRFSource() {
		super("HackRF", "TSDRPlugin_HackRF", false);
	}

	private static int findIndex(long[] array, long value) {
		for (int i = 0; i < array.length; i++)
			if (array[i] == value) return i;
		return -1;
	}

	private static ParsedParams parseParams(String params) {
		ParsedParams p = new ParsedParams();
		if (params == null || params.trim().isEmpty()) return p;

		String[] tokens = params.trim().split("\\s+");
		for (String token : tokens) {
			int eq = token.indexOf('=');
			if (eq < 0) continue;
			String key = token.substring(0, eq);
			String value = token.substring(eq + 1);

			try {
				if (key.equals("samplerate") || key.equals("rate") || key.equals("sr")) {
					p.samplerate = Long.parseLong(value);
				} else if (key.equals("bandwidth") || key.equals("bw")) {
					p.bandwidth = Long.parseLong(value);
				} else if (key.equals("amp") || key.equals("amp_enable")) {
					p.amp = Integer.parseInt(value) != 0;
				}
			} catch (NumberFormatException e) {}
		}
		return p;
	}

	private static String buildParams(long samplerate, long bandwidth, boolean amp) {
		StringBuilder sb = new StringBuilder();
		sb.append("samplerate=").append(samplerate);
		if (bandwidth > 0)
			sb.append(" bandwidth=").append(bandwidth);
		if (amp)
			sb.append(" amp=1");
		return sb.toString();
	}

	@Override
	public boolean populateGUI(final Container cont, final String defaultprefs, final ActionListenerRegistrator okbutton) {
		final ParsedParams parsed = parseParams(defaultprefs);

		final JLabel lblSampleRate = new JLabel("Sample rate:");
		cont.add(lblSampleRate);
		lblSampleRate.setBounds(12, 8, 90, 24);

		final JComboBox<String> cbSampleRate = new JComboBox<>();
		cbSampleRate.setModel(new DefaultComboBoxModel<>(SAMPLE_RATE_LABELS));
		int srIdx = findIndex(SAMPLE_RATES, parsed.samplerate);
		cbSampleRate.setSelectedIndex(srIdx >= 0 ? srIdx : DEFAULT_SAMPLE_RATE_INDEX);
		cbSampleRate.setBounds(105, 8, 150, 24);
		cont.add(cbSampleRate);

		final JLabel lblBandwidth = new JLabel("Bandwidth:");
		cont.add(lblBandwidth);
		lblBandwidth.setBounds(270, 8, 80, 24);

		final JComboBox<String> cbBandwidth = new JComboBox<>();
		cbBandwidth.setModel(new DefaultComboBoxModel<>(BANDWIDTH_LABELS));
		int bwIdx = findIndex(BANDWIDTHS, parsed.bandwidth);
		cbBandwidth.setSelectedIndex(bwIdx >= 0 ? bwIdx : DEFAULT_BANDWIDTH_INDEX);
		cbBandwidth.setBounds(355, 8, 170, 24);
		cont.add(cbBandwidth);

		final JCheckBox chkAmp = new JCheckBox("AMP enable", parsed.amp);
		chkAmp.setBounds(12, 40, 120, 24);
		cont.add(chkAmp);

		okbutton.addActionListener(new ActionListener() {
			@Override
			public void actionPerformed(ActionEvent arg0) {
				long sr = SAMPLE_RATES[cbSampleRate.getSelectedIndex()];
				long bw = BANDWIDTHS[cbBandwidth.getSelectedIndex()];
				boolean amp = chkAmp.isSelected();
				setParams(buildParams(sr, bw, amp));
			}
		});

		return true;
	}

	private static class ParsedParams {
		long samplerate = 20000000;
		long bandwidth = 0;
		boolean amp = false;
	}
}
