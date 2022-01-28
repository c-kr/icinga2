/* Icinga 2 | (c) 2012 Icinga GmbH | GPLv2+ */

#include "icinga/host.hpp"
#include <BoostTestTargetConfig.h>
#include <iostream>
#include <sstream>
#include <unordered_map>
#include <utility>
#include <vector>

using namespace icinga;

BOOST_AUTO_TEST_SUITE(icinga_checkresult)

static CheckResult::Ptr MakeCheckResult(ServiceState state)
{
	CheckResult::Ptr cr = new CheckResult();

	cr->SetState(state);

	double now = Utility::GetTime();
	cr->SetScheduleStart(now);
	cr->SetScheduleEnd(now);
	cr->SetExecutionStart(now);
	cr->SetExecutionEnd(now);

	return cr;
}

static void NotificationHandler(const Checkable::Ptr& checkable, NotificationType type)
{
	std::cout << "Notification triggered: " << Notification::NotificationTypeToString(type) << std::endl;

	checkable->SetExtension("requested_notifications", true);
	checkable->SetExtension("notification_type", type);
}

static void CheckNotification(const Checkable::Ptr& checkable, bool expected, NotificationType type = NotificationRecovery)
{
	BOOST_CHECK((expected && checkable->GetExtension("requested_notifications").ToBool()) || (!expected && !checkable->GetExtension("requested_notifications").ToBool()));

	if (expected && checkable->GetExtension("requested_notifications").ToBool())
		BOOST_CHECK(checkable->GetExtension("notification_type") == type);

	checkable->SetExtension("requested_notifications", false);
}

BOOST_AUTO_TEST_CASE(host_1attempt)
{
	boost::signals2::connection c = Checkable::OnNotificationsRequested.connect([](const Checkable::Ptr& checkable, NotificationType type,
		const CheckResult::Ptr&, const String&, const String&, const MessageOrigin::Ptr&) {
		NotificationHandler(checkable, type);
	});

	Host::Ptr host = new Host();
	host->SetActive(true);
	host->SetMaxCheckAttempts(1);
	host->Activate();
	host->SetAuthority(true);
	host->SetStateRaw(ServiceOK);
	host->SetStateType(StateTypeHard);

	std::cout << "Before first check result (ok, hard)" << std::endl;
	BOOST_CHECK(host->GetState() == HostUp);
	BOOST_CHECK(host->GetStateType() == StateTypeHard);
	BOOST_CHECK(host->GetCheckAttempt() == 1);
	BOOST_CHECK(host->IsReachable() == true);
	CheckNotification(host, false);

	std::cout << "First check result (unknown)" << std::endl;
	host->ProcessCheckResult(MakeCheckResult(ServiceUnknown));
	BOOST_CHECK(host->GetState() == HostDown);
	BOOST_CHECK(host->GetStateType() == StateTypeHard);
	BOOST_CHECK(host->GetCheckAttempt() == 1);
	BOOST_CHECK(host->IsReachable() == true);
	CheckNotification(host, true, NotificationProblem);

	std::cout << "Second check result (ok)" << std::endl;
	host->ProcessCheckResult(MakeCheckResult(ServiceOK));
	BOOST_CHECK(host->GetState() == HostUp);
	BOOST_CHECK(host->GetStateType() == StateTypeHard);
	BOOST_CHECK(host->GetCheckAttempt() == 1);
	BOOST_CHECK(host->IsReachable() == true);
	CheckNotification(host, true, NotificationRecovery);

	std::cout << "Third check result (critical)" << std::endl;
	host->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	BOOST_CHECK(host->GetState() == HostDown);
	BOOST_CHECK(host->GetStateType() == StateTypeHard);
	BOOST_CHECK(host->GetCheckAttempt() == 1);
	BOOST_CHECK(host->IsReachable() == true);
	CheckNotification(host, true, NotificationProblem);

	std::cout << "Fourth check result (ok)" << std::endl;
	host->ProcessCheckResult(MakeCheckResult(ServiceOK));
	BOOST_CHECK(host->GetState() == HostUp);
	BOOST_CHECK(host->GetStateType() == StateTypeHard);
	BOOST_CHECK(host->GetCheckAttempt() == 1);
	BOOST_CHECK(host->IsReachable() == true);
	CheckNotification(host, true, NotificationRecovery);

	c.disconnect();
}

BOOST_AUTO_TEST_CASE(host_2attempts)
{
	boost::signals2::connection c = Checkable::OnNotificationsRequested.connect([](const Checkable::Ptr& checkable, NotificationType type,
		const CheckResult::Ptr&, const String&, const String&, const MessageOrigin::Ptr&) {
		NotificationHandler(checkable, type);
	});

	Host::Ptr host = new Host();
	host->SetActive(true);
	host->SetMaxCheckAttempts(2);
	host->Activate();
	host->SetAuthority(true);
	host->SetStateRaw(ServiceOK);
	host->SetStateType(StateTypeHard);

	std::cout << "Before first check result (ok, hard)" << std::endl;
	BOOST_CHECK(host->GetState() == HostUp);
	BOOST_CHECK(host->GetStateType() == StateTypeHard);
	BOOST_CHECK(host->GetCheckAttempt() == 1);
	BOOST_CHECK(host->IsReachable() == true);
	CheckNotification(host, false);

	std::cout << "First check result (unknown)" << std::endl;
	host->ProcessCheckResult(MakeCheckResult(ServiceUnknown));
	BOOST_CHECK(host->GetState() == HostDown);
	BOOST_CHECK(host->GetStateType() == StateTypeSoft);
	BOOST_CHECK(host->GetCheckAttempt() == 1);
	BOOST_CHECK(host->IsReachable() == true);
	CheckNotification(host, false);

	std::cout << "Second check result (critical)" << std::endl;
	host->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	BOOST_CHECK(host->GetState() == HostDown);
	BOOST_CHECK(host->GetStateType() == StateTypeHard);
	BOOST_CHECK(host->GetCheckAttempt() == 1);
	BOOST_CHECK(host->IsReachable() == true);
	CheckNotification(host, true, NotificationProblem);

	std::cout << "Third check result (ok)" << std::endl;
	host->ProcessCheckResult(MakeCheckResult(ServiceOK));
	BOOST_CHECK(host->GetState() == HostUp);
	BOOST_CHECK(host->GetStateType() == StateTypeHard);
	BOOST_CHECK(host->GetCheckAttempt() == 1);
	BOOST_CHECK(host->IsReachable() == true);
	CheckNotification(host, true, NotificationRecovery);

	std::cout << "Fourth check result (critical)" << std::endl;
	host->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	BOOST_CHECK(host->GetState() == HostDown);
	BOOST_CHECK(host->GetStateType() == StateTypeSoft);
	BOOST_CHECK(host->GetCheckAttempt() == 1);
	BOOST_CHECK(host->IsReachable() == true);
	CheckNotification(host, false);

	std::cout << "Fifth check result (ok)" << std::endl;
	host->ProcessCheckResult(MakeCheckResult(ServiceOK));
	BOOST_CHECK(host->GetState() == HostUp);
	BOOST_CHECK(host->GetStateType() == StateTypeHard);
	BOOST_CHECK(host->GetCheckAttempt() == 1);
	BOOST_CHECK(host->IsReachable() == true);
	CheckNotification(host, false);

	c.disconnect();
}

BOOST_AUTO_TEST_CASE(host_3attempts)
{
	boost::signals2::connection c = Checkable::OnNotificationsRequested.connect([](const Checkable::Ptr& checkable, NotificationType type,
		const CheckResult::Ptr&, const String&, const String&, const MessageOrigin::Ptr&) {
		NotificationHandler(checkable, type);
	});

	Host::Ptr host = new Host();
	host->SetActive(true);
	host->SetMaxCheckAttempts(3);
	host->Activate();
	host->SetAuthority(true);
	host->SetStateRaw(ServiceOK);
	host->SetStateType(StateTypeHard);

	std::cout << "Before first check result (ok, hard)" << std::endl;
	BOOST_CHECK(host->GetState() == HostUp);
	BOOST_CHECK(host->GetStateType() == StateTypeHard);
	BOOST_CHECK(host->GetCheckAttempt() == 1);
	BOOST_CHECK(host->IsReachable() == true);
	CheckNotification(host, false);

	std::cout << "First check result (unknown)" << std::endl;
	host->ProcessCheckResult(MakeCheckResult(ServiceUnknown));
	BOOST_CHECK(host->GetState() == HostDown);
	BOOST_CHECK(host->GetStateType() == StateTypeSoft);
	BOOST_CHECK(host->GetCheckAttempt() == 1);
	BOOST_CHECK(host->IsReachable() == true);
	CheckNotification(host, false);

	std::cout << "Second check result (critical)" << std::endl;
	host->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	BOOST_CHECK(host->GetState() == HostDown);
	BOOST_CHECK(host->GetStateType() == StateTypeSoft);
	BOOST_CHECK(host->GetCheckAttempt() == 2);
	BOOST_CHECK(host->IsReachable() == true);
	CheckNotification(host, false);

	std::cout << "Third check result (critical)" << std::endl;
	host->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	BOOST_CHECK(host->GetState() == HostDown);
	BOOST_CHECK(host->GetStateType() == StateTypeHard);
	BOOST_CHECK(host->GetCheckAttempt() == 1);
	BOOST_CHECK(host->IsReachable() == true);
	CheckNotification(host, true, NotificationProblem);

	std::cout << "Fourth check result (ok)" << std::endl;
	host->ProcessCheckResult(MakeCheckResult(ServiceOK));
	BOOST_CHECK(host->GetState() == HostUp);
	BOOST_CHECK(host->GetStateType() == StateTypeHard);
	BOOST_CHECK(host->GetCheckAttempt() == 1);
	BOOST_CHECK(host->IsReachable() == true);
	CheckNotification(host, true, NotificationRecovery);

	std::cout << "Fifth check result (critical)" << std::endl;
	host->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	BOOST_CHECK(host->GetState() == HostDown);
	BOOST_CHECK(host->GetStateType() == StateTypeSoft);
	BOOST_CHECK(host->GetCheckAttempt() == 1);
	BOOST_CHECK(host->IsReachable() == true);
	CheckNotification(host, false);

	std::cout << "Sixth check result (ok)" << std::endl;
	host->ProcessCheckResult(MakeCheckResult(ServiceOK));
	BOOST_CHECK(host->GetState() == HostUp);
	BOOST_CHECK(host->GetStateType() == StateTypeHard);
	BOOST_CHECK(host->GetCheckAttempt() == 1);
	BOOST_CHECK(host->IsReachable() == true);
	CheckNotification(host, false);

	c.disconnect();
}

BOOST_AUTO_TEST_CASE(service_1attempt)
{
	boost::signals2::connection c = Checkable::OnNotificationsRequested.connect([](const Checkable::Ptr& checkable, NotificationType type,
		const CheckResult::Ptr&, const String&, const String&, const MessageOrigin::Ptr&) {
		NotificationHandler(checkable, type);
	});

	Service::Ptr service = new Service();
	service->SetActive(true);
	service->SetMaxCheckAttempts(1);
	service->Activate();
	service->SetAuthority(true);
	service->SetStateRaw(ServiceOK);
	service->SetStateType(StateTypeHard);

	std::cout << "Before first check result (ok, hard)" << std::endl;
	BOOST_CHECK(service->GetState() == ServiceOK);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetCheckAttempt() == 1);
	BOOST_CHECK(service->IsReachable() == true);
	CheckNotification(service, false);

	std::cout << "First check result (unknown)" << std::endl;
	service->ProcessCheckResult(MakeCheckResult(ServiceUnknown));
	BOOST_CHECK(service->GetState() == ServiceUnknown);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetCheckAttempt() == 1);
	BOOST_CHECK(service->IsReachable() == true);
	CheckNotification(service, true, NotificationProblem);

	std::cout << "Second check result (ok)" << std::endl;
	service->ProcessCheckResult(MakeCheckResult(ServiceOK));
	BOOST_CHECK(service->GetState() == ServiceOK);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetCheckAttempt() == 1);
	BOOST_CHECK(service->IsReachable() == true);
	CheckNotification(service, true, NotificationRecovery);

	std::cout << "Third check result (critical)" << std::endl;
	service->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	BOOST_CHECK(service->GetState() == ServiceCritical);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetCheckAttempt() == 1);
	BOOST_CHECK(service->IsReachable() == true);
	CheckNotification(service, true, NotificationProblem);

	std::cout << "Fourth check result (ok)" << std::endl;
	service->ProcessCheckResult(MakeCheckResult(ServiceOK));
	BOOST_CHECK(service->GetState() == ServiceOK);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetCheckAttempt() == 1);
	BOOST_CHECK(service->IsReachable() == true);
	CheckNotification(service, true, NotificationRecovery);

	c.disconnect();
}

BOOST_AUTO_TEST_CASE(service_2attempts)
{
	boost::signals2::connection c = Checkable::OnNotificationsRequested.connect([](const Checkable::Ptr& checkable, NotificationType type,
		const CheckResult::Ptr&, const String&, const String&, const MessageOrigin::Ptr&) {
		NotificationHandler(checkable, type);
	});

	Service::Ptr service = new Service();
	service->SetActive(true);
	service->SetMaxCheckAttempts(2);
	service->Activate();
	service->SetAuthority(true);
	service->SetStateRaw(ServiceOK);
	service->SetStateType(StateTypeHard);

	std::cout << "Before first check result (ok, hard)" << std::endl;
	BOOST_CHECK(service->GetState() == ServiceOK);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetCheckAttempt() == 1);
	BOOST_CHECK(service->IsReachable() == true);
	CheckNotification(service, false);

	std::cout << "First check result (unknown)" << std::endl;
	service->ProcessCheckResult(MakeCheckResult(ServiceUnknown));
	BOOST_CHECK(service->GetState() == ServiceUnknown);
	BOOST_CHECK(service->GetStateType() == StateTypeSoft);
	BOOST_CHECK(service->GetCheckAttempt() == 1);
	BOOST_CHECK(service->IsReachable() == true);
	CheckNotification(service, false);

	std::cout << "Second check result (critical)" << std::endl;
	service->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	BOOST_CHECK(service->GetState() == ServiceCritical);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetCheckAttempt() == 1);
	BOOST_CHECK(service->IsReachable() == true);
	CheckNotification(service, true, NotificationProblem);

	std::cout << "Third check result (ok)" << std::endl;
	service->ProcessCheckResult(MakeCheckResult(ServiceOK));
	BOOST_CHECK(service->GetState() == ServiceOK);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetCheckAttempt() == 1);
	BOOST_CHECK(service->IsReachable() == true);
	CheckNotification(service, true, NotificationRecovery);

	std::cout << "Fourth check result (critical)" << std::endl;
	service->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	BOOST_CHECK(service->GetState() == ServiceCritical);
	BOOST_CHECK(service->GetStateType() == StateTypeSoft);
	BOOST_CHECK(service->GetCheckAttempt() == 1);
	BOOST_CHECK(service->IsReachable() == true);
	CheckNotification(service, false);

	std::cout << "Fifth check result (ok)" << std::endl;
	service->ProcessCheckResult(MakeCheckResult(ServiceOK));
	BOOST_CHECK(service->GetState() == ServiceOK);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetCheckAttempt() == 1);
	BOOST_CHECK(service->IsReachable() == true);
	CheckNotification(service, false);

	c.disconnect();
}

BOOST_AUTO_TEST_CASE(service_3attempts)
{
	boost::signals2::connection c = Checkable::OnNotificationsRequested.connect([](const Checkable::Ptr& checkable, NotificationType type,
		const CheckResult::Ptr&, const String&, const String&, const MessageOrigin::Ptr&) {
		NotificationHandler(checkable, type);
	});

	Service::Ptr service = new Service();
	service->SetActive(true);
	service->SetMaxCheckAttempts(3);
	service->Activate();
	service->SetAuthority(true);
	service->SetStateRaw(ServiceOK);
	service->SetStateType(StateTypeHard);

	std::cout << "Before first check result (ok, hard)" << std::endl;
	BOOST_CHECK(service->GetState() == ServiceOK);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetCheckAttempt() == 1);
	BOOST_CHECK(service->IsReachable() == true);
	CheckNotification(service, false);

	std::cout << "First check result (unknown)" << std::endl;
	service->ProcessCheckResult(MakeCheckResult(ServiceUnknown));
	BOOST_CHECK(service->GetState() == ServiceUnknown);
	BOOST_CHECK(service->GetStateType() == StateTypeSoft);
	BOOST_CHECK(service->GetCheckAttempt() == 1);
	BOOST_CHECK(service->IsReachable() == true);
	CheckNotification(service, false);

	std::cout << "Second check result (critical)" << std::endl;
	service->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	BOOST_CHECK(service->GetState() == ServiceCritical);
	BOOST_CHECK(service->GetStateType() == StateTypeSoft);
	BOOST_CHECK(service->GetCheckAttempt() == 2);
	BOOST_CHECK(service->IsReachable() == true);
	CheckNotification(service, false);

	std::cout << "Third check result (critical)" << std::endl;
	service->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	BOOST_CHECK(service->GetState() == ServiceCritical);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetCheckAttempt() == 1);
	BOOST_CHECK(service->IsReachable() == true);
	CheckNotification(service, true, NotificationProblem);

	std::cout << "Fourth check result (ok)" << std::endl;
	service->ProcessCheckResult(MakeCheckResult(ServiceOK));
	BOOST_CHECK(service->GetState() == ServiceOK);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetCheckAttempt() == 1);
	BOOST_CHECK(service->IsReachable() == true);
	CheckNotification(service, true, NotificationRecovery);

	std::cout << "Fifth check result (critical)" << std::endl;
	service->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	BOOST_CHECK(service->GetState() == ServiceCritical);
	BOOST_CHECK(service->GetStateType() == StateTypeSoft);
	BOOST_CHECK(service->GetCheckAttempt() == 1);
	BOOST_CHECK(service->IsReachable() == true);
	CheckNotification(service, false);

	std::cout << "Sixth check result (ok)" << std::endl;
	service->ProcessCheckResult(MakeCheckResult(ServiceOK));
	BOOST_CHECK(service->GetState() == ServiceOK);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetCheckAttempt() == 1);
	BOOST_CHECK(service->IsReachable() == true);
	CheckNotification(service, false);

	c.disconnect();
}

BOOST_AUTO_TEST_CASE(host_flapping_notification)
{
#ifndef I2_DEBUG
	BOOST_WARN_MESSAGE(false, "This test can only be run in a debug build!");
#else /* I2_DEBUG */
	boost::signals2::connection c = Checkable::OnNotificationsRequested.connect([](const Checkable::Ptr& checkable, NotificationType type,
		const CheckResult::Ptr&, const String&, const String&, const MessageOrigin::Ptr&) {
		NotificationHandler(checkable, type);
	});

	int timeStepInterval = 60;

	Host::Ptr host = new Host();
	host->SetActive(true);
	host->Activate();
	host->SetAuthority(true);
	host->SetStateRaw(ServiceOK);
	host->SetStateType(StateTypeHard);
	host->SetEnableFlapping(true);

	/* Initialize start time */
	Utility::SetTime(0);

	std::cout << "Before first check result (ok, hard)" << std::endl;
	BOOST_CHECK(host->GetState() == HostUp);
	BOOST_CHECK(host->GetStateType() == StateTypeHard);
	BOOST_CHECK(host->GetCheckAttempt() == 1);

	Utility::IncrementTime(timeStepInterval);

	std::cout << "Inserting flapping check results" << std::endl;

	for (int i = 0; i < 10; i++) {
		ServiceState state = (i % 2 == 0 ? ServiceOK : ServiceCritical);
		host->ProcessCheckResult(MakeCheckResult(state));
		Utility::IncrementTime(timeStepInterval);
	}

	BOOST_CHECK(host->IsFlapping() == true);

	CheckNotification(host, true, NotificationFlappingStart);

	std::cout << "Now calm down..." << std::endl;

	for (int i = 0; i < 20; i++) {
		host->ProcessCheckResult(MakeCheckResult(ServiceOK));
		Utility::IncrementTime(timeStepInterval);
	}

	CheckNotification(host, true, NotificationFlappingEnd);


	c.disconnect();

#endif /* I2_DEBUG */
}

BOOST_AUTO_TEST_CASE(service_flapping_notification)
{
#ifndef I2_DEBUG
	BOOST_WARN_MESSAGE(false, "This test can only be run in a debug build!");
#else /* I2_DEBUG */
	boost::signals2::connection c = Checkable::OnNotificationsRequested.connect([](const Checkable::Ptr& checkable, NotificationType type,
		const CheckResult::Ptr&, const String&, const String&, const MessageOrigin::Ptr&) {
		NotificationHandler(checkable, type);
	});

	int timeStepInterval = 60;

	Service::Ptr service = new Service();
	service->SetActive(true);
	service->Activate();
	service->SetAuthority(true);
	service->SetStateRaw(ServiceOK);
	service->SetStateType(StateTypeHard);
	service->SetEnableFlapping(true);

	/* Initialize start time */
	Utility::SetTime(0);

	std::cout << "Before first check result (ok, hard)" << std::endl;
	BOOST_CHECK(service->GetState() == ServiceOK);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetCheckAttempt() == 1);

	Utility::IncrementTime(timeStepInterval);

	std::cout << "Inserting flapping check results" << std::endl;

	for (int i = 0; i < 10; i++) {
		ServiceState state = (i % 2 == 0 ? ServiceOK : ServiceCritical);
		service->ProcessCheckResult(MakeCheckResult(state));
		Utility::IncrementTime(timeStepInterval);
	}

	BOOST_CHECK(service->IsFlapping() == true);

	CheckNotification(service, true, NotificationFlappingStart);



	std::cout << "Now calm down..." << std::endl;

	for (int i = 0; i < 20; i++) {
		service->ProcessCheckResult(MakeCheckResult(ServiceOK));
		Utility::IncrementTime(timeStepInterval);
	}

	CheckNotification(service, true, NotificationFlappingEnd);

	c.disconnect();

#endif /* I2_DEBUG */
}

BOOST_AUTO_TEST_CASE(service_flapping_problem_notifications)
{
#ifndef I2_DEBUG
	BOOST_WARN_MESSAGE(false, "This test can only be run in a debug build!");
#else /* I2_DEBUG */
	boost::signals2::connection c = Checkable::OnNotificationsRequested.connect([](const Checkable::Ptr& checkable, NotificationType type,
		const CheckResult::Ptr&, const String&, const String&, const MessageOrigin::Ptr&) {
		NotificationHandler(checkable, type);
	});

	int timeStepInterval = 60;

	Service::Ptr service = new Service();
	service->Activate();
	service->SetAuthority(true);
	service->SetStateRaw(ServiceOK);
	service->SetStateType(StateTypeHard);
	service->SetEnableFlapping(true);
	service->SetMaxCheckAttempts(3);

	/* Initialize start time */
	Utility::SetTime(0);

	std::cout << "Before first check result (ok, hard)" << std::endl;
	BOOST_CHECK(service->GetState() == ServiceOK);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetCheckAttempt() == 1);

	Utility::IncrementTime(timeStepInterval);

	std::cout << "Inserting flapping check results" << std::endl;

	for (int i = 0; i < 10; i++) {
		ServiceState state = (i % 2 == 0 ? ServiceOK : ServiceCritical);
		service->ProcessCheckResult(MakeCheckResult(state));
		Utility::IncrementTime(timeStepInterval);
	}

	BOOST_CHECK(service->IsFlapping() == true);

	CheckNotification(service, true, NotificationFlappingStart);

	//Insert enough check results to get into hard problem state but staying flapping

	service->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	Utility::IncrementTime(timeStepInterval);
	service->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	Utility::IncrementTime(timeStepInterval);
	service->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	Utility::IncrementTime(timeStepInterval);


	BOOST_CHECK(service->IsFlapping() == true);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetState() == ServiceCritical);

	CheckNotification(service, false, NotificationProblem);

	// Calm down
	while (service->IsFlapping()) {
		service->ProcessCheckResult(MakeCheckResult(ServiceCritical));
		Utility::IncrementTime(timeStepInterval);
	}

	CheckNotification(service, true, NotificationFlappingEnd);

	/* Intended behaviour is a Problem notification being sent as well, but there are is a Problem:
	 * We don't know whether the Object was Critical before we started flapping and sent out a Notification.
	 * A notification will not be sent, no matter how many criticals follow.
	 *
	 * service->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	 * CheckNotification(service, true, NotificationProblem);
	 * ^ This fails, no notification will be sent
	 *
	 * There is also a different issue, when we receive a OK check result, a Recovery Notification will be sent
	 * since the service went from hard critical into soft ok. Yet there is no fitting critical notification.
	 * This should not happen:
	 *
	 * service->ProcessCheckResult(MakeCheckResult(ServiceOK));
	 * CheckNotification(service, false, NotificationRecovery);
	 * ^ This fails, recovery is sent
	 */

	BOOST_CHECK(service->IsFlapping() == false);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetState() == ServiceCritical);

	// Known failure, see #5713
	// CheckNotification(service, true, NotificationProblem);

	service->ProcessCheckResult(MakeCheckResult(ServiceOK));
	Utility::IncrementTime(timeStepInterval);

	// Known failure, see #5713
	// CheckNotification(service, true, NotificationRecovery);

	c.disconnect();

#endif /* I2_DEBUG */
}

BOOST_AUTO_TEST_CASE(service_flapping_ok_into_bad)
{
#ifndef I2_DEBUG
	BOOST_WARN_MESSAGE(false, "This test can only be run in a debug build!");
#else /* I2_DEBUG */
	boost::signals2::connection c = Checkable::OnNotificationsRequested.connect([](const Checkable::Ptr& checkable, NotificationType type,
		const CheckResult::Ptr&, const String&, const String&, const MessageOrigin::Ptr&) {
		NotificationHandler(checkable, type);
	});

	int timeStepInterval = 60;

	Service::Ptr service = new Service();
	service->Activate();
	service->SetAuthority(true);
	service->SetStateRaw(ServiceOK);
	service->SetStateType(StateTypeHard);
	service->SetEnableFlapping(true);
	service->SetMaxCheckAttempts(3);

	/* Initialize start time */
	Utility::SetTime(0);

	std::cout << "Before first check result (ok, hard)" << std::endl;
	BOOST_CHECK(service->GetState() == ServiceOK);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetCheckAttempt() == 1);

	Utility::IncrementTime(timeStepInterval);

	std::cout << "Inserting flapping check results" << std::endl;

	for (int i = 0; i < 10; i++) {
		ServiceState state = (i % 2 == 0 ? ServiceOK : ServiceCritical);
		service->ProcessCheckResult(MakeCheckResult(state));
		Utility::IncrementTime(timeStepInterval);
	}

	BOOST_CHECK(service->IsFlapping() == true);

	CheckNotification(service, true, NotificationFlappingStart);

	//Insert enough check results to get into hard problem state but staying flapping

	service->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	Utility::IncrementTime(timeStepInterval);
	service->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	Utility::IncrementTime(timeStepInterval);
	service->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	Utility::IncrementTime(timeStepInterval);


	BOOST_CHECK(service->IsFlapping() == true);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetState() == ServiceCritical);

	CheckNotification(service, false, NotificationProblem);

	// Calm down
	while (service->IsFlapping()) {
		service->ProcessCheckResult(MakeCheckResult(ServiceCritical));
		Utility::IncrementTime(timeStepInterval);
	}

	CheckNotification(service, true, NotificationFlappingEnd);

	service->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	Utility::IncrementTime(timeStepInterval);

	BOOST_CHECK(service->IsFlapping() == false);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetState() == ServiceCritical);

	// We expect a problem notification here
	// Known failure, see #5713
	// CheckNotification(service, true, NotificationProblem);

	c.disconnect();

#endif /* I2_DEBUG */
}
BOOST_AUTO_TEST_CASE(service_flapping_ok_over_bad_into_ok)
{
#ifndef I2_DEBUG
	BOOST_WARN_MESSAGE(false, "This test can only be run in a debug build!");
#else /* I2_DEBUG */
	boost::signals2::connection c = Checkable::OnNotificationsRequested.connect([](const Checkable::Ptr& checkable, NotificationType type,
		const CheckResult::Ptr&, const String&, const String&, const MessageOrigin::Ptr&) {
		NotificationHandler(checkable, type);
	});

	int timeStepInterval = 60;

	Service::Ptr service = new Service();
	service->Activate();
	service->SetAuthority(true);
	service->SetStateRaw(ServiceOK);
	service->SetStateType(StateTypeHard);
	service->SetEnableFlapping(true);
	service->SetMaxCheckAttempts(3);

	/* Initialize start time */
	Utility::SetTime(0);

	std::cout << "Before first check result (ok, hard)" << std::endl;
	BOOST_CHECK(service->GetState() == ServiceOK);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetCheckAttempt() == 1);

	Utility::IncrementTime(timeStepInterval);

	std::cout << "Inserting flapping check results" << std::endl;

	for (int i = 0; i < 10; i++) {
		ServiceState state = (i % 2 == 0 ? ServiceOK : ServiceCritical);
		service->ProcessCheckResult(MakeCheckResult(state));
		Utility::IncrementTime(timeStepInterval);
	}

	BOOST_CHECK(service->IsFlapping() == true);

	CheckNotification(service, true, NotificationFlappingStart);

	//Insert enough check results to get into hard problem state but staying flapping

	service->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	Utility::IncrementTime(timeStepInterval);
	service->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	Utility::IncrementTime(timeStepInterval);
	service->ProcessCheckResult(MakeCheckResult(ServiceCritical));
	Utility::IncrementTime(timeStepInterval);


	BOOST_CHECK(service->IsFlapping() == true);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetState() == ServiceCritical);

	CheckNotification(service, false, NotificationProblem);

	// Calm down
	while (service->IsFlapping()) {
		service->ProcessCheckResult(MakeCheckResult(ServiceCritical));
		Utility::IncrementTime(timeStepInterval);
	}

	CheckNotification(service, true, NotificationFlappingEnd);

	service->ProcessCheckResult(MakeCheckResult(ServiceOK));
	Utility::IncrementTime(timeStepInterval);

	BOOST_CHECK(service->IsFlapping() == false);
	BOOST_CHECK(service->GetStateType() == StateTypeHard);
	BOOST_CHECK(service->GetState() == ServiceOK);

	// There should be no recovery
	// Known failure, see #5713
	// CheckNotification(service, false, NotificationRecovery);

	c.disconnect();

#endif /* I2_DEBUG */
}

class StateSequenceGenerator {
public:
	StateSequenceGenerator(size_t num, std::vector<ServiceState> availableStates)
		: availableStates(std::move(availableStates)), progress(num), done(false) {}

	std::vector<ServiceState> operator()() {
		if (done) {
			return {};
		}

		std::vector<ServiceState> result(progress.size());

		for (size_t i = 0; i < progress.size(); i++) {
			result[i] = availableStates[progress[i]];
		}

		bool overflow = true;
		for (size_t i = 0; i < progress.size(); i++) {
			if (++progress[i] < availableStates.size()) {
				overflow = false;
				break;
			} else {
				progress[i] = 0;
			}
		}
		done = overflow;

		return result;
	}

	explicit operator bool() const {
		return !done;
	}

	void reset() {
		std::fill(progress.begin(), progress.end(), 0);
		done = false;
	}

private:
	std::vector<ServiceState> availableStates;
	std::vector<size_t> progress;
	bool done;
};

BOOST_AUTO_TEST_CASE(suppressed_notification)
{
#ifndef I2_DEBUG
	BOOST_WARN_MESSAGE(false, "This test can only be run in a debug build!");
#else /* I2_DEBUG */

	std::unordered_map<Checkable::Ptr, std::vector<std::pair<NotificationType, ServiceState>>> requestedNotifications;

	auto c = Checkable::OnNotificationsRequested.connect([&requestedNotifications](
		const Checkable::Ptr& checkable, NotificationType type,	const CheckResult::Ptr& cr,
		const String&, const String&, const MessageOrigin::Ptr&
	) {
		std::cout << "  -> OnNotificationsRequested(" << Notification::NotificationTypeToString(type) << ", " << Service::StateToString(cr->GetState()) << ")" << std::endl;
		requestedNotifications[checkable].emplace_back(type, cr->GetState());
	});

	auto assertNotifications = [&requestedNotifications](
		const Checkable::Ptr& checkable,
		const std::vector<std::pair<NotificationType, ServiceState>>& expected,
		const std::string& extraMessage
	) {
		auto pretty = [](const std::vector<std::pair<NotificationType, ServiceState>>& vec) {
			std::ostringstream s;

			s << "{";
			bool first = true;
			for (const auto &v : vec) {
				if (first) {
					first = false;
				} else {
					s << ", ";
				}
				s << Notification::NotificationTypeToString(v.first)
						  << "/" << Service::StateToString(v.second);
			}
			s << "}";

			return s.str();
		};

		auto& got = requestedNotifications[checkable];
		BOOST_CHECK_MESSAGE(got == expected, "expected=" << pretty(expected)
			<< " got=" << pretty(got) << (extraMessage.empty() ? "" : " ") << extraMessage);

		requestedNotifications.erase(checkable);
	};

	StateSequenceGenerator stateSeqGen(6, {ServiceOK, ServiceWarning, ServiceCritical, ServiceUnknown});

	for (bool isVolatile : {false, true}) {
		for (int checkAttempts : {1, 2}) {
			stateSeqGen.reset();
			while (stateSeqGen) {
				const std::vector<ServiceState> sequence = stateSeqGen();

				std::string testcase;
				{
					std::ostringstream buf;
					buf << "volatile=" << isVolatile << " checkAttempts=" << checkAttempts << " sequence={";
					bool first = true;
					for (ServiceState s: sequence) {
						if (!first) {
							buf << " ";
						}
						buf << Service::StateToString(s);
						first = false;
					}
					buf << "}";
					testcase = buf.str();
				}
				std::cout << "Test case: " << testcase << std::endl;

				Service::Ptr service = new Service();
				service->SetActive(true);
				service->SetVolatile(isVolatile);
				service->SetMaxCheckAttempts(checkAttempts);
				service->Activate();
				service->SetAuthority(true);
				service->SetStateRaw(sequence.front());
				service->SetStateType(StateTypeHard);
				service->SetEnableActiveChecks(false); // TODO: maybe needed due to LikelyToBeCheckedSoon
				BOOST_CHECK(service->GetState() == sequence.front());
				BOOST_CHECK(service->GetStateType() == StateTypeHard);

				std::cout << "  Downtime Start" << std::endl;
				Downtime::Ptr downtime = new Downtime();
				downtime->SetCheckable(service);
				downtime->SetFixed(true);
				downtime->SetStartTime(Utility::GetTime() - 3600);
				downtime->SetEndTime(Utility::GetTime() + 3600);
				service->RegisterDowntime(downtime);
				BOOST_CHECK(service->IsInDowntime());

				bool first = true;
				for (ServiceState s: sequence) {
					if (first) {
						// Don't process check result for initial state as it was set above.
						first = false;
						continue;
					}
					std::cout << "  ProcessCheckResult(" << Service::StateToString(s) << ")" << std::endl;
					service->ProcessCheckResult(MakeCheckResult(s));
					BOOST_CHECK(service->GetState() == s);
					if (checkAttempts == 1) {
						BOOST_CHECK(service->GetStateType() == StateTypeHard);
					}
				}

				assertNotifications(service, {}, "(no notifications in downtime)");

				BOOST_CHECK(!service->GetSuppressedNotifications() || service->GetStateBeforeSuppression() == sequence.front());

				std::cout << "  Downtime End" << std::endl;
				service->UnregisterDowntime(downtime);
				BOOST_CHECK(!service->IsInDowntime());

				std::cout << "  FireSuppressedNotifications()" << std::endl;
				service->FireSuppressedNotifications();

				if (service->GetStateType() == icinga::StateTypeSoft) {
					assertNotifications(service, {}, testcase + " (should not fire in soft state)");

					for (int i = 0; i < checkAttempts && service->GetStateType() == StateTypeSoft; i++) {
						std::cout << "  ProcessCheckResult(" << Service::StateToString(sequence.back()) << ")" << std::endl;
						service->ProcessCheckResult(MakeCheckResult(sequence.back()));
						BOOST_CHECK(service->GetState() == sequence.back());
					}

					std::cout << "  FireSuppressedNotifications()" << std::endl;
					service->FireSuppressedNotifications();
				}

				BOOST_CHECK(service->GetStateType() == StateTypeHard);

				if (sequence.front() != sequence.back()) {
					assertNotifications(service, {
						{sequence.back() == ServiceOK ? NotificationRecovery : NotificationProblem, sequence.back()}
					}, testcase);
				} else {
					assertNotifications(service, {}, testcase);
				}
			}
		}
	}

	c.disconnect();
#endif /* I2_DEBUG */
}
BOOST_AUTO_TEST_SUITE_END()
