/*************************************************************************

    Exidy Vertigo hardware

*************************************************************************/

#include "emu.h"
#include "machine/74148.h"
#include "machine/pit8253.h"
#include "includes/vertigo.h"


/*************************************
 *
 *  Prototypes
 *
 *************************************/






/*************************************
 *
 *  Statics
 *
 *************************************/

/* Timestamp of last INTL4 change. The vector CPU runs for
   the delta between this and now.
*/

/* State of the priority encoder output */

/* Result of the last ADC channel sampled */

/* 8254 timer config */
const struct pit8253_interface vertigo_pit8254_config =
{
	{
		{
			240000,
			DEVCB_NULL,
			DEVCB_DRIVER_LINE_MEMBER(vertigo_state,v_irq4_w)
		}, {
			240000,
			DEVCB_NULL,
			DEVCB_DRIVER_LINE_MEMBER(vertigo_state,v_irq3_w)
		}, {
			240000,
			DEVCB_NULL,
			DEVCB_NULL
		}
	}
};



/*************************************
 *
 *  IRQ handling. The priority encoder
 *  has to be emulated. Otherwise
 *  interrupts are lost.
 *
 *************************************/

void vertigo_update_irq(device_t *device)
{
	vertigo_state *state = device->machine().driver_data<vertigo_state>();
	if (state->m_irq_state < 7)
		state->m_maincpu->set_input_line(state->m_irq_state ^ 7, CLEAR_LINE);

	state->m_irq_state = ttl74148_output_r(device);

	if (state->m_irq_state < 7)
		state->m_maincpu->set_input_line(state->m_irq_state ^ 7, ASSERT_LINE);
}


void vertigo_state::update_irq_encoder(int line, int state)
{
	ttl74148_input_line_w(m_ttl74148, line, !state);
	ttl74148_update(m_ttl74148);
}


WRITE_LINE_MEMBER(vertigo_state::v_irq4_w)
{
	update_irq_encoder(INPUT_LINE_IRQ4, state);
	vertigo_vproc(m_maincpu->attotime_to_cycles(machine().time() - m_irq4_time), state);
	m_irq4_time = machine().time();
}


WRITE_LINE_MEMBER(vertigo_state::v_irq3_w)
{
	if (state)
		m_audiocpu->set_input_line(INPUT_LINE_IRQ0, ASSERT_LINE);

	update_irq_encoder(INPUT_LINE_IRQ3, state);
}



/*************************************
 *
 *  ADC and coin handlers
 *
 *************************************/

READ16_MEMBER(vertigo_state::vertigo_io_convert)
{
	static const char *const adcnames[] = { "P1X", "P1Y", "PADDLE" };

	if (offset > 2)
		m_adc_result = 0;
	else
		m_adc_result = ioport(adcnames[offset])->read();

	update_irq_encoder(INPUT_LINE_IRQ2, ASSERT_LINE);
	return 0;
}


READ16_MEMBER(vertigo_state::vertigo_io_adc)
{
	update_irq_encoder(INPUT_LINE_IRQ2, CLEAR_LINE);
	return m_adc_result;
}


READ16_MEMBER(vertigo_state::vertigo_coin_r)
{
	update_irq_encoder(INPUT_LINE_IRQ6, CLEAR_LINE);
	return (ioport("COIN")->read());
}


INTERRUPT_GEN_MEMBER(vertigo_state::vertigo_interrupt)
{
	/* Coin inputs cause IRQ6 */
	if ((ioport("COIN")->read() & 0x7) < 0x7)
		update_irq_encoder(INPUT_LINE_IRQ6, ASSERT_LINE);
}



/*************************************
 *
 *  Sound board interface
 *
 *************************************/

WRITE16_MEMBER(vertigo_state::vertigo_wsot_w)
{
	/* Reset sound cpu */
	if ((data & 2) == 0)
		m_audiocpu->set_input_line(INPUT_LINE_RESET, ASSERT_LINE);
	else
		m_audiocpu->set_input_line(INPUT_LINE_RESET, CLEAR_LINE);
}


TIMER_CALLBACK_MEMBER(vertigo_state::sound_command_w)
{
	m_custom->exidy440_sound_command(param);

	/* It is important that the sound cpu ACKs the sound command
	   quickly. Otherwise the main CPU gives up with sound. Boosting
	   the interleave for a while helps. */

	machine().scheduler().boost_interleave(attotime::zero, attotime::from_usec(100));
}


WRITE16_MEMBER(vertigo_state::vertigo_audio_w)
{
	if (ACCESSING_BITS_0_7)
		machine().scheduler().synchronize(timer_expired_delegate(FUNC(vertigo_state::sound_command_w),this), data & 0xff);
}


READ16_MEMBER(vertigo_state::vertigo_sio_r)
{
	return m_custom->exidy440_sound_command_ack() ? 0xfc : 0xfd;
}



/*************************************
 *
 *  Machine initialization
 *
 *************************************/

void vertigo_state::machine_start()
{
	m_ttl74148 = machine().device("74148");

	save_item(NAME(m_irq_state));
	save_item(NAME(m_adc_result));
	save_item(NAME(m_irq4_time));

	vertigo_vproc_init();
}

void vertigo_state::machine_reset()
{
	int i;

	ttl74148_enable_input_w(m_ttl74148, 0);

	for (i = 0; i < 8; i++)
		ttl74148_input_line_w(m_ttl74148, i, 1);

	ttl74148_update(m_ttl74148);
	vertigo_vproc_reset();

	m_irq4_time = machine().time();
	m_irq_state = 7;
}



/*************************************
 *
 *  Motor controller interface
 *
 *************************************/

WRITE16_MEMBER(vertigo_state::vertigo_motor_w)
{
	/* Motor controller interface. Not emulated. */
}
